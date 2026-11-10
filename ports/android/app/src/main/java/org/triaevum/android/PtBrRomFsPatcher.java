package org.triaevum.android;

import android.content.Context;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Aplica o pacote de tradução em Português do Brasil (PT-BR) diretamente na RomFS
 * no dispositivo Android, sem necessidade de ferramentas externas ou PC.
 */
public final class PtBrRomFsPatcher {

    private static final String TAG = "PtBrRomFsPatcher";
    private static final int INVALID = 0xFFFFFFFF;
    public static final long EXPECTED_PTBR_ROMFS_SIZE = 478385348L;

    public interface ProgressCallback {
        void onProgress(String message, int percentage);
    }

    public static class RomFsFileInfo {
        public final String path;
        public final long metaAbs;
        public final long payloadRel;
        public final long absolute;
        public final long size;

        public RomFsFileInfo(String path, long metaAbs, long payloadRel, long absolute, long size) {
            this.path = path;
            this.metaAbs = metaAbs;
            this.payloadRel = payloadRel;
            this.absolute = absolute;
            this.size = size;
        }
    }

    public static Map<String, RomFsFileInfo> indexRomFs(RandomAccessFile raf, long[] offsetsOut) throws IOException {
        raf.seek(0);
        byte[] header = new byte[40];
        raf.readFully(header);

        long headerSize = readU32(header, 0);
        if (headerSize != 40) {
            throw new IOException("Header Level-3 inesperado no RomFS: " + headerSize);
        }

        long dirMetaOffset = readU32(header, 12);
        long dirMetaSize = readU32(header, 16);
        long fileMetaOffset = readU32(header, 28);
        long fileMetaSize = readU32(header, 32);
        long fileDataOffset = readU32(header, 36);

        if (offsetsOut != null && offsetsOut.length >= 2) {
            offsetsOut[0] = fileMetaOffset;
            offsetsOut[1] = fileDataOffset;
        }

        byte[] dirs = new byte[(int) dirMetaSize];
        raf.seek(dirMetaOffset);
        raf.readFully(dirs);

        byte[] files = new byte[(int) fileMetaSize];
        raf.seek(fileMetaOffset);
        raf.readFully(files);

        Map<Integer, String> dirPaths = new HashMap<>();

        class StackItem {
            final int offset;
            final String parentPath;
            StackItem(int o, String p) { offset = o; parentPath = p; }
        }

        ArrayDeque<StackItem> stack = new ArrayDeque<>();
        stack.push(new StackItem(0, ""));

        while (!stack.isEmpty()) {
            StackItem item = stack.pop();
            int off = item.offset;
            if (off == INVALID || dirPaths.containsKey(off) || off + 24 > dirs.length) continue;

            long sibling = readU32(dirs, off + 4);
            long childDir = readU32(dirs, off + 8);
            long nameSize = readU32(dirs, off + 20);

            if (off + 24 + (int) nameSize > dirs.length) {
                throw new IOException("Nome de diretório fora dos limites.");
            }

            String name = new String(dirs, off + 24, (int) nameSize, StandardCharsets.UTF_16LE);
            String current;
            if (!item.parentPath.isEmpty() && !name.isEmpty()) {
                current = item.parentPath + "/" + name;
            } else if (!name.isEmpty()) {
                current = name;
            } else {
                current = item.parentPath;
            }

            dirPaths.put(off, current);

            if (childDir != INVALID) {
                stack.push(new StackItem((int) childDir, current));
            }
            if (sibling != INVALID) {
                stack.push(new StackItem((int) sibling, item.parentPath));
            }
        }

        Map<String, RomFsFileInfo> result = new HashMap<>();

        for (Map.Entry<Integer, String> entry : dirPaths.entrySet()) {
            int dirOff = entry.getKey();
            String dirPath = entry.getValue();
            if (dirOff + 16 > dirs.length) continue;

            long childFile = readU32(dirs, dirOff + 12);
            long current = childFile;

            while (current != INVALID) {
                int fo = (int) current;
                if (fo + 32 > files.length) break;

                long sibling = readU32(files, fo + 4);
                long payloadRel = readU64(files, fo + 8);
                long size = readU64(files, fo + 16);
                long nameSize = readU32(files, fo + 28);

                if (fo + 32 + (int) nameSize > files.length) break;

                String fileName = new String(files, fo + 32, (int) nameSize, StandardCharsets.UTF_16LE);
                String fullPath = dirPath.isEmpty() ? fileName : (dirPath + "/" + fileName);

                result.put(fullPath.toLowerCase(), new RomFsFileInfo(
                        fullPath,
                        fileMetaOffset + fo,
                        payloadRel,
                        fileDataOffset + payloadRel,
                        size
                ));

                current = sibling;
            }
        }

        return result;
    }

    public static boolean applyTranslationPack(Context context, File baseRomFs, File outputRomFs, ProgressCallback callback) {
        try {
            if (callback != null) callback.onProgress("Preparando imagem PT-BR...", 5);

            if (outputRomFs.exists() && outputRomFs.length() == EXPECTED_PTBR_ROMFS_SIZE) {
                Log.i(TAG, "romfs.pt.bin já existe e possui tamanho esperado.");
                return true;
            }

            // 1. Copiar baseRomFs para outputRomFs
            if (callback != null) callback.onProgress("Copiando base do RomFS...", 15);
            copyFile(baseRomFs, outputRomFs);

            // 2. Indexar RomFS
            if (callback != null) callback.onProgress("Indexando recursos do jogo...", 35);
            long[] offsets = new long[2]; // [0] = fileMetaOffset, [1] = fileDataOffset
            Map<String, RomFsFileInfo> index;
            try (RandomAccessFile raf = new RandomAccessFile(outputRomFs, "rw")) {
                index = indexRomFs(raf, offsets);
                long fileDataOffset = offsets[1];

                // 3. Abrir o zip embutido nos assets
                if (callback != null) callback.onProgress("Aplicando tradução PT-BR...", 45);
                try (InputStream assetIn = context.getAssets().open("translation/oot3d_ptbr_pack.zip");
                     ZipInputStream zis = new ZipInputStream(new BufferedInputStream(assetIn))) {

                    ZipEntry entry;
                    int appliedCount = 0;
                    byte[] buffer = new byte[16384];

                    while ((entry = zis.getNextEntry()) != null) {
                        String targetPath = entry.getName().toLowerCase();
                        RomFsFileInfo info = index.get(targetPath);
                        if (info == null) {
                            zis.closeEntry();
                            continue;
                        }

                        ByteArrayOutputStream baos = new ByteArrayOutputStream();
                        int read;
                        while ((read = zis.read(buffer)) > 0) {
                            baos.write(buffer, 0, read);
                        }
                        byte[] payload = baos.toByteArray();

                        if (payload.length == info.size) {
                            // Substituição direta in-place
                            raf.seek(info.absolute);
                            raf.write(payload);
                        } else {
                            // Relocação para o final do arquivo alinhada a 16 bytes
                            long eof = raf.length();
                            long aligned = (eof + 15) & ~15L;
                            if (aligned > eof) {
                                raf.seek(eof);
                                byte[] pad = new byte[(int)(aligned - eof)];
                                raf.write(pad);
                            }
                            long finalAbs = aligned;
                            long finalRel = finalAbs - fileDataOffset;
                            raf.seek(finalAbs);
                            raf.write(payload);

                            // Atualiza metadata: +8 = payloadRel (U64), +16 = size (U64)
                            raf.seek(info.metaAbs + 8);
                            writeU64LE(raf, finalRel);
                            raf.seek(info.metaAbs + 16);
                            writeU64LE(raf, payload.length);
                        }

                        appliedCount++;
                        if (callback != null && appliedCount % 10 == 0) {
                            int pct = 45 + (appliedCount * 45 / 126);
                            callback.onProgress(String.format("Aplicando tradução (%d/126)...", appliedCount), pct);
                        }
                        zis.closeEntry();
                    }

                    Log.i(TAG, "Tradução PT-BR aplicada com sucesso! Total operações: " + appliedCount);
                }
            }

            if (callback != null) callback.onProgress("Finalizando tradução...", 95);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Falha ao aplicar tradução PT-BR", e);
            return false;
        }
    }

    private static void copyFile(File src, File dst) throws IOException {
        try (FileInputStream in = new FileInputStream(src);
             FileOutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[65536];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            out.flush();
        }
    }

    private static long readU32(byte[] data, int offset) {
        return ((long) (data[offset] & 0xFF))
                | (((long) (data[offset + 1] & 0xFF)) << 8)
                | (((long) (data[offset + 2] & 0xFF)) << 16)
                | (((long) (data[offset + 3] & 0xFF)) << 24);
    }

    private static long readU64(byte[] data, int offset) {
        return readU32(data, offset) | (readU32(data, offset + 4) << 32);
    }

    private static void writeU64LE(RandomAccessFile raf, long val) throws IOException {
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

