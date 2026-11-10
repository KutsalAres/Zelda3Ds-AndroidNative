package org.triaevum.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public final class PtBrRomFsPatcherTest {

    @Rule
    public TemporaryFolder mFolder = new TemporaryFolder();

    @Test
    public void testRomFsIndexingAndRelocation() throws Exception {
        File romfsFile = mFolder.newFile("test_romfs.bin");

        // Construir um RomFS Level-3 mock simples:
        // Header (40 bytes)
        // dir_meta: 1 raiz + 1 subdiretório "menu"
        // file_meta: 1 arquivo "menu/hud_all00.ctxb"
        // data: payload original de 8 bytes

        byte[] dirData = buildMockDirMeta();
        byte[] fileData = buildMockFileMeta();
        byte[] payloadData = "ORIGINAL".getBytes(StandardCharsets.US_ASCII);

        int dirMetaOffset = 40;
        int dirMetaSize = dirData.length;
        int fileMetaOffset = dirMetaOffset + dirMetaSize;
        int fileMetaSize = fileData.length;
        int dataOffset = fileMetaOffset + fileMetaSize;

        try (RandomAccessFile raf = new RandomAccessFile(romfsFile, "rw")) {
            // Escrever Header
            writeU32(raf, 40); // headerSize
            writeU32(raf, 0);  // dirHashOffset
            writeU32(raf, 0);  // dirHashSize
            writeU32(raf, dirMetaOffset);
            writeU32(raf, dirMetaSize);
            writeU32(raf, 0);  // fileHashOffset
            writeU32(raf, 0);  // fileHashSize
            writeU32(raf, fileMetaOffset);
            writeU32(raf, fileMetaSize);
            writeU32(raf, dataOffset);

            // Escrever Dirs
            raf.write(dirData);
            // Escrever Files
            raf.write(fileData);
            // Escrever Payload
            raf.write(payloadData);
        }

        // 1. Validar Indexação
        long[] offsets = new long[2];
        Map<String, PtBrRomFsPatcher.RomFsFileInfo> index;
        try (RandomAccessFile raf = new RandomAccessFile(romfsFile, "r")) {
            index = PtBrRomFsPatcher.indexRomFs(raf, offsets);
        }

        assertEquals(fileMetaOffset, offsets[0]);
        assertEquals(dataOffset, offsets[1]);
        assertNotNull(index.get("menu/hud_all00.ctxb"));

        PtBrRomFsPatcher.RomFsFileInfo info = index.get("menu/hud_all00.ctxb");
        assertEquals(8, info.size);
        assertEquals(dataOffset, info.absolute);

        // 2. Simular patch com payload de tamanho diferente (relocação)
        byte[] newPayload = "NOVO_PAYLOAD_MAIOR_QUE_8_BYTES".getBytes(StandardCharsets.US_ASCII);
        try (RandomAccessFile raf = new RandomAccessFile(romfsFile, "rw")) {
            long eof = raf.length();
            long aligned = (eof + 15) & ~15L;
            if (aligned > eof) {
                raf.seek(eof);
                raf.write(new byte[(int) (aligned - eof)]);
            }
            long finalRel = aligned - dataOffset;
            raf.seek(aligned);
            raf.write(newPayload);

            // Atualiza metadata
            raf.seek(info.metaAbs + 8);
            writeU64LE(raf, finalRel);
            raf.seek(info.metaAbs + 16);
            writeU64LE(raf, newPayload.length);
        }

        // 3. Re-indexar e validar
        Map<String, PtBrRomFsPatcher.RomFsFileInfo> updatedIndex;
        try (RandomAccessFile raf = new RandomAccessFile(romfsFile, "r")) {
            updatedIndex = PtBrRomFsPatcher.indexRomFs(raf, offsets);
        }

        PtBrRomFsPatcher.RomFsFileInfo updatedInfo = updatedIndex.get("menu/hud_all00.ctxb");
        assertNotNull(updatedInfo);
        assertEquals(newPayload.length, updatedInfo.size);
        assertTrue(updatedInfo.absolute >= dataOffset + 8);

        // Validar leitura do novo payload
        byte[] readBack = new byte[newPayload.length];
        try (RandomAccessFile raf = new RandomAccessFile(romfsFile, "r")) {
            raf.seek(updatedInfo.absolute);
            raf.readFully(readBack);
        }
        assertEquals(new String(newPayload, StandardCharsets.US_ASCII),
                     new String(readBack, StandardCharsets.US_ASCII));
    }

    private static byte[] buildMockDirMeta() throws Exception {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        // Dir 0 (root): parent=0, sibling=INVALID, childDir=dir1, childFile=INVALID, hashNext=INVALID, nameSize=0
        writeU32(baos, 0);
        writeU32(baos, 0xFFFFFFFF);
        writeU32(baos, 24); // childDir aponta para offset 24
        writeU32(baos, 0xFFFFFFFF);
        writeU32(baos, 0xFFFFFFFF);
        writeU32(baos, 0);

        // Dir 1 ("menu"): parent=0, sibling=INVALID, childDir=INVALID, childFile=file0, hashNext=INVALID, name="menu" (8 bytes UTF-16LE)
        byte[] name = "menu".getBytes(StandardCharsets.UTF_16LE);
        writeU32(baos, 0);
        writeU32(baos, 0xFFFFFFFF);
        writeU32(baos, 0xFFFFFFFF);
        writeU32(baos, 0); // childFile aponta para file 0
        writeU32(baos, 0xFFFFFFFF);
        writeU32(baos, name.length);
        baos.write(name);
        return baos.toByteArray();
    }

    private static byte[] buildMockFileMeta() throws Exception {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        // File 0 ("hud_all00.ctxb"): parent=24 (dir 1), sibling=INVALID, payloadRel=0, size=8, hashNext=INVALID, name="hud_all00.ctxb"
        byte[] name = "hud_all00.ctxb".getBytes(StandardCharsets.UTF_16LE);
        writeU32(baos, 24);
        writeU32(baos, 0xFFFFFFFF);
        writeU64(baos, 0); // payloadRel = 0
        writeU64(baos, 8); // size = 8
        writeU32(baos, 0xFFFFFFFF);
        writeU32(baos, name.length);
        baos.write(name);
        return baos.toByteArray();
    }

    private static void writeU32(ByteArrayOutputStream baos, int val) {
        baos.write(val & 0xFF);
        baos.write((val >> 8) & 0xFF);
        baos.write((val >> 16) & 0xFF);
        baos.write((val >> 24) & 0xFF);
    }

    private static void writeU64(ByteArrayOutputStream baos, long val) {
        writeU32(baos, (int) (val & 0xFFFFFFFF));
        writeU32(baos, (int) ((val >> 32) & 0xFFFFFFFF));
    }

    private static void writeU32(RandomAccessFile raf, int val) throws Exception {
        byte[] b = new byte[4];
        b[0] = (byte) (val & 0xFF);
        b[1] = (byte) ((val >> 8) & 0xFF);
        b[2] = (byte) ((val >> 16) & 0xFF);
        b[3] = (byte) ((val >> 24) & 0xFF);
        raf.write(b);
    }

    private static void writeU64LE(RandomAccessFile raf, long val) throws Exception {
        byte[] b = new byte[8];
        b[0] = (byte) (val & 0xFF);
        b[1] = (byte) ((val >> 8) & 0xFF);
        b[2] = (byte) ((val >> 16) & 0xFF);
        b[3] = (byte) ((val >> 24) & 0xFF);
        b[4] = (byte) ((val >> 32) & 0xFF);
        b[5] = (byte) ((val >> 40) & 0xFF);
        b[6] = (byte) ((val >> 48) & 0xFF);
        b[7] = (byte) ((val >> 56) & 0xFF);
        raf.write(b);
    }
}

