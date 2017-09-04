/******************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd
** All rights reserved.
** For any questions to The Qt Company, please use contact form at http://qt.io
**
** This file is part of the Qt Virtual Keyboard module.
**
** Licensees holding valid commercial license for Qt may use this file in
** accordance with the Qt License Agreement provided with the Software
** or, alternatively, in accordance with the terms contained in a written
** agreement between you and The Qt Company.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.io
**
******************************************************************************/

import java.io.BufferedInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class dict2qt {

    public static void main(String[] args) {
        boolean showHelp = false;
        boolean showUsage = false;
        boolean littleEndian = false;
        String outputFileName = "";
        String inputFileName = "";
        File inputFile = null;

        if (args.length > 0) {
            for (int i = 0; i < args.length; i++) {
                if (args[i].startsWith("-")) {
                    if (args[i].compareTo("-h") == 0) {
                        showHelp = true;
                        showUsage = true;
                        break;
                    } else if (args[i].compareTo("-o") == 0) {
                        if (++i >= args.length) {
                            System.err.println("Error: missing argument <output file>");
                            showUsage = true;
                            break;
                        }
                        outputFileName = args[i];
                    } else if (args[i].compareTo("-le") == 0) {
                        littleEndian = true;
                    } else {
                        System.err.println("Error: unknown option '" + args[i] + "'");
                        showUsage = true;
                        break;
                    }
                } else if (inputFileName.isEmpty() && i + 1 == args.length) {
                    inputFileName = args[i];
                } else {
                    System.err.println("Error: unexpected argument '" + args[i] + "'");
                    showUsage = true;
                    break;
                }
            }

            if (!showUsage && !showHelp) {
                if (!inputFileName.isEmpty()) {
                    inputFile = new File(inputFileName);
                    if (!inputFile.exists()) {
                        System.err.println("Error: input file does not exist '" + inputFileName + "'");
                        return;
                    }
                    if (outputFileName.isEmpty())
                        outputFileName = inputFileName;
                }

                if (inputFile == null) {
                    System.err.println("Error: missing argument file");
                    showUsage = true;
                }
            }
        } else {
            showUsage = true;
        }

        if (showUsage || showHelp) {
            if (showHelp) {
                System.err.println("TCIME dictionary converter for Qt Virtual Keyboard");
                System.err.println("");
                System.err.println("Copyright (C) 2015 The Qt Company Ltd - All rights reserved.");
                System.err.println("");
                System.err.println("  This utility converts TCIME dictionaries to Qt compatible");
                System.err.println("  format. The dictionaries are basically Java char[][] arrays");
                System.err.println("  serialized to file with ObjectOutputStream.");
                System.err.println("");
                System.err.println("  The corresponding data format in the Qt dictionary is");
                System.err.println("  QVector<QVector<ushort>>. The byte order is set to big endian");
                System.err.println("  by default, but can be changed with -le option.");
            }
            if (showUsage) {
                System.err.println("");
                System.err.println("Usage: java dict2qt [options] file");
                System.err.println("Options:");
                System.err.println("  -o <output file>         Place the output into <output file>");
                System.err.println("  -le                      Change byte order to little endian");
                System.err.println("  -h                       Display help");
            }
            return;
        }

        char[][] dictionary = null;
        try {
            dictionary = loadDictionary(new FileInputStream(inputFile));
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return;
        }
        if (dictionary == null)
            return;

        int dictionarySize = calculateDictionarySize(dictionary);
        ByteBuffer buffer = ByteBuffer.allocate(dictionarySize);
        if (littleEndian)
            buffer.order(ByteOrder.LITTLE_ENDIAN);
        else
            buffer.order(ByteOrder.BIG_ENDIAN);
        buffer.putInt(dictionary.length);
        for (int i = 0; i < dictionary.length; i++) {
            char[] data = dictionary[i];
            if (data != null) {
                buffer.putInt(data.length);
                for (int j = 0; j < data.length; j++) {
                    buffer.putChar(data[j]);
                }
            } else {
                buffer.putInt(0);
            }
        }

        byte[] bytes = buffer.array();
        DataOutputStream dos = null;
        try {
            File outputFile = new File(outputFileName);
            FileOutputStream os = new FileOutputStream(outputFile);
            dos = new DataOutputStream(os);
            dos.write(bytes, 0, buffer.position());
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (dos != null) {
            try {
                dos.close();
              } catch (IOException e) {
                  e.printStackTrace();
              }
            }
        }
    }

    static char[][] loadDictionary(InputStream ins) {
        char[][] result = null;
        ObjectInputStream oin = null;
        try {
          BufferedInputStream bis = new BufferedInputStream(ins);
          oin = new ObjectInputStream(bis);
          result = (char[][]) oin.readObject();
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
          if (oin != null) {
            try {
              oin.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
          }
        }
        return result;
    }

    static int calculateDictionarySize(final char[][] dictionary) {
        int result = 4;
        for (int i = 0; i < dictionary.length; i++) {
            char[] data = dictionary[i];
            result += 4;
            if (data != null)
                result += data.length * 2;
        }
        return result;
    }

}
