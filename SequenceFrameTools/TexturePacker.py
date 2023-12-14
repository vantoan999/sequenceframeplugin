import multiprocessing
import os
import shutil
import struct
import sys

import lz4.frame
import zlib

from PIL import Image
from PIL import ImageOps
import inspect

'''
Format | C Type | Python Type | Standard Size
l      | long   | integer     | 4 bytes
'''
CONST_LONG_BYTES = 4
CONST_PKM_HEADER_BYTES = 16


'''
WARNING: Check the Kanzi documentation for "GraphicsFormat" when port this script to other versions of Kanzi.
'''
# textureFormat: GraphicsFormatETC2_R8G8B8A8_UNORM = 34 (Kanzi 3.6.5)

GraphicsFormatETC2_R8G8B8A8_UNORM = 34

'''
WARNING: The maximum value of dataOffset (long) in the C++ program should be larger than the package size.
'''
texturePackageHeader = {"sizeOffset": 0, "dataOffset": 0, "textureNumber": 480, "textureWidth": 960, "textureHeight": 540, "textureFormat": 34, "compressionAlgorithm": 1}
texturePackageHeader["sizeOffset"] = CONST_LONG_BYTES * len(texturePackageHeader)
texturePackageHeader["dataOffset"] = texturePackageHeader["sizeOffset"] + CONST_LONG_BYTES * texturePackageHeader["textureNumber"]

compressionList = [ {"enum": 1, "suffix": ".lz4" },
                    {"enum": 2, "suffix": ".zlib"} ]

formattedDirectoryName = "texture_{0:06d}"
formattedImageName = "frame_{0:06d}"
imageSuffix = ".png"
imageStartIndex = 0
imageEndIndex = imageStartIndex + texturePackageHeader["textureNumber"]

intermediateSuffix = ".tga"

def log(msg):
    frame = inspect.currentframe().f_back
    func_name = frame.f_code.co_name
    line_no = frame.f_lineno
    print(f'{func_name}()::{line_no}| {msg}')

"""
TO seperate images and tools to folders
in: images
out: multiple foldlers that each contains images and tools for image processing 
"""
def preprocess(startIndex, endIndex, dirName):
    log("===================================================== Preprocess.\n")
    log(f'pID {multiprocessing.current_process().pid} works on {startIndex} to {endIndex} in folder {dirName}')
    os.mkdir(dirName)
    
    toolList = ["etcpack.exe", "imconv.exe"]
    for tool in toolList:
        try:
            shutil.copy(tool, os.path.join(dirName, tool))
            log(f'copy {tool} to {dirName} SUCCESS')
        except Exception as e:
            log(f'copy {tool} to {dirName} FAILED: {e}')

    for i in range(startIndex, endIndex):
        imageName = formattedImageName.format(i)
        try:
            shutil.move(imageName + imageSuffix, os.path.join(dirName, imageName + imageSuffix))
            log(f'move {imageName + imageSuffix} to {dirName} SUCCESS')
        except Exception as e:
            log(f'move {imageName + imageSuffix} to {dirName} FAILED: {e}')
    print("preprocess done ++++++++++++++++++++++++++++++++++++++++ " + dirName)

'''
TO compress every image
in: images and tools
out: every compressed image in *pkm format
- before compressing in pkm, images compressed in *.tga with Pillow (PIL)
- then images compressed in *.pkm with ETCPACK
'''
def createTextures(startIndex, endIndex, dirName, width, height):
    print("================================================ Create textures.\n")
    os.chdir(dirName)
    log(os.getcwd())

    for i in range(startIndex, endIndex):
        imageName = formattedImageName.format(i)
        log(f'imageName = {imageName + imageSuffix}')
        with Image.open(imageName + imageSuffix, "r") as im:
            try:
                im = im.resize((width, height))
                im = ImageOps.flip(im)
                im.save(imageName + intermediateSuffix)
                log(f"Resize & FLip {imageName} and save as {imageName + intermediateSuffix} SUCCESS")
            except Exception as e:
                log(f"Resize & FLip {imageName} and save as {imageName + intermediateSuffix} FAILED: {e}")
        os.system(f"etcpack -c etc2 -f RGBA8 {imageName + intermediateSuffix} {imageName}_ETC2_RGBA8.pkm -v on")
        log(f"etcpack -c etc2 -f RGBA8 {imageName + intermediateSuffix} {imageName}_ETC2_RGBA8.pkm -v on")

    os.chdir("..")
    print("createTextures done ++++++++++++++++++++++++++++++++++++ " + dirName)

'''
TO compress every *.pkm to *.lz4 and *.zlib
in: pkm
out: lz4
- use lib lz4 to compress 
'''
def compressTextures(startIndex, endIndex, dirName):
    print("============================================== Compress textures.\n")
    os.chdir(dirName)

    for i in range(startIndex, endIndex):
        imageName = formattedImageName.format(i)

        with open(imageName + "_ETC2_RGBA8.pkm", "rb") as inFile:
            inFile.seek(CONST_PKM_HEADER_BYTES, 0) # Discard header information.
            textureFile = inFile.read()

            for compression in compressionList:
                textureCompressedFile = 0
                if (compression["suffix"] == ".lz4"):
                    textureCompressedFile = lz4.frame.compress(textureFile, compression_level = lz4.frame.COMPRESSIONLEVEL_MAX)
                elif (compression["suffix"] == ".zlib"):
                    textureCompressedFile = zlib.compress(textureFile)

                with open(imageName + "_ETC2_RGBA8.pkm" + compression["suffix"], "wb") as outFile:
                    outFile.write(textureCompressedFile)

    os.chdir("..")
    print("compressTextures done ++++++++++++++++++++++++++++++++++ " + dirName)

'''
TO pack every lz4 to an final result
'''
def packTextures(startIndex, endIndex):
    print("================================================== Pack textures.\n")

    for compression in compressionList:
        with open("_ETC2_RGBA8" + compression["suffix"], "wb") as outFile: # Clear the file if already exsited.
            texturePackageHeader["textureFormat"] = GraphicsFormatETC2_R8G8B8A8_UNORM
            texturePackageHeader["compressionAlgorithm"] = compression["enum"]

            for key, value in texturePackageHeader.items():
                outFile.write(struct.pack("l", value))

            for i in range(startIndex, endIndex):
                outFile.write(struct.pack("l", 0)) # Placeholders

            headerOffset = texturePackageHeader["sizeOffset"]
            dataOffset = texturePackageHeader["dataOffset"]

            for i in range(startIndex, endIndex):
                imageName = formattedImageName.format(i)

                textureName = imageName + "_ETC2_RGBA8.pkm" + compression["suffix"]
                compressedSize = os.path.getsize(textureName)
                
                with open(textureName, "rb") as inFile:
                    outFile.seek(0, 2)
                    outFile.write(inFile.read())
                    inFile.seek(0, 2)
                    dataOffset += inFile.tell()

                outFile.seek(headerOffset, 0)
                outFile.write(struct.pack("l", dataOffset))
                headerOffset += CONST_LONG_BYTES

    print("packTextures done +++++++++++++++++++++++++++++++++++++++++++++++++")

'''
TO move all *.lz4 to an parent folder
'''
def postprocess(startIndex, endIndex, dirName):
    print("==================================================== Postprocess.\n")

    for i in range(startIndex, endIndex):
        imageName = formattedImageName.format(i)
        shutil.move(os.path.join(dirName, imageName + imageSuffix), imageName + imageSuffix)

        for compression in compressionList:
            textureName = imageName + "_ETC2_RGBA8.pkm" + compression["suffix"]
            shutil.move(os.path.join(dirName, textureName), textureName)

    print("postprocess done +++++++++++++++++++++++++++++++++++++++ " + dirName)

def cleaning(startIndex, endIndex):
    print("======================================================= Cleaning.\n")

    os.mkdir(imageSuffix)

    for i in range(startIndex, endIndex):
        imageName = formattedImageName.format(i)
        shutil.move(imageName + imageSuffix, os.path.join(imageSuffix, imageName + imageSuffix))

    for compression in compressionList:
        # dirName = "_ETC2_RGBA8" + "_" + compression["suffix"][1:]
        # os.mkdir(dirName)

        for i in range(startIndex, endIndex):
            imageName = formattedImageName.format(i)
            textureName = imageName + "_ETC2_RGBA8.pkm" + compression["suffix"]
            os.remove(textureName)
            # shutil.move(textureName, os.path.join(dirName, textureName))

    print("cleaning done +++++++++++++++++++++++++++++++++++++++++++++++++++++")


if __name__ == '__main__':
    multiprocessing.freeze_support()
    
    if (4 == len(sys.argv)):
        texturePackageHeader["textureNumber"] = int(sys.argv[1])
        texturePackageHeader["textureWidth"] = int(sys.argv[2])
        texturePackageHeader["textureHeight"] = int(sys.argv[3])
        texturePackageHeader["sizeOffset"] = CONST_LONG_BYTES * len(texturePackageHeader)
        texturePackageHeader["dataOffset"] = texturePackageHeader["sizeOffset"] + CONST_LONG_BYTES * texturePackageHeader["textureNumber"]
        imageEndIndex = imageStartIndex + texturePackageHeader["textureNumber"]
    print("=====textureNumber = ", texturePackageHeader["textureNumber"])
    print("=====textureWidth = ", texturePackageHeader["textureWidth"])
    print("=====textureHeight = ", texturePackageHeader["textureHeight"])
    print("=====sizeOffset = ", texturePackageHeader["sizeOffset"])
    print("=====dataOffset = ", texturePackageHeader["dataOffset"])
    print("=====imageEndIndex = ", imageEndIndex)

    directoryNumber = int(multiprocessing.cpu_count() * 10)
    workloadPerDirectory = int(texturePackageHeader["textureNumber"] / directoryNumber)
    workloadMore = int(texturePackageHeader["textureNumber"] % directoryNumber)

    pool = multiprocessing.Pool()
    for i in range(0, directoryNumber):
        dirName = formattedDirectoryName.format(i)
        log(f'dirName = {dirName}')
        startIndex = imageStartIndex + i * (workloadPerDirectory + 1)
        endIndex = imageStartIndex + (i + 1) * (workloadPerDirectory + 1)
        if (i >= workloadMore):
            startIndex = imageStartIndex + i * workloadPerDirectory + workloadMore
            endIndex = imageStartIndex + (i + 1) * workloadPerDirectory  + workloadMore
            
        pool.apply_async(preprocess, (startIndex, endIndex, dirName, ))    
    pool.close()
    pool.join()

    pool = multiprocessing.Pool()
    for i in range(0, directoryNumber):
        dirName = formattedDirectoryName.format(i)
        startIndex = imageStartIndex + i * (workloadPerDirectory + 1)
        endIndex = imageStartIndex + (i + 1) * (workloadPerDirectory + 1)
        if (i >= workloadMore):
            startIndex = imageStartIndex + i * workloadPerDirectory + workloadMore
            endIndex = imageStartIndex + (i + 1) * workloadPerDirectory  + workloadMore
        pool.apply_async(createTextures, (startIndex, endIndex, dirName, texturePackageHeader["textureWidth"], texturePackageHeader["textureHeight"] ))        
    # i=3
    # startIndex = imageStartIndex + i * (workloadPerDirectory + 1)
    # endIndex = imageStartIndex + (i + 1) * (workloadPerDirectory + 1)
    # dirName = formattedDirectoryName.format(i)
    # log(f"startIndex= {startIndex}")
    # log(f"endIndex= {endIndex}")
    # log(f"dirName= {dirName}")
    # # pool.apply(createTextures, (startIndex, endIndex, dirName, ))  
    # createTextures(startIndex, endIndex, dirName, texturePackageHeader["textureWidth"], texturePackageHeader["textureHeight"] )
    pool.close()
    pool.join()

    pool = multiprocessing.Pool()
    for i in range(0, directoryNumber):
        dirName = formattedDirectoryName.format(i)
        startIndex = imageStartIndex + i * (workloadPerDirectory + 1)
        endIndex = imageStartIndex + (i + 1) * (workloadPerDirectory + 1)
        if (i >= workloadMore):
            startIndex = imageStartIndex + i * workloadPerDirectory + workloadMore
            endIndex = imageStartIndex + (i + 1) * workloadPerDirectory  + workloadMore
        pool.apply_async(compressTextures, (startIndex, endIndex, dirName, ))  
    pool.close()
    pool.join()

    pool = multiprocessing.Pool()
    for i in range(0, directoryNumber):
        dirName = formattedDirectoryName.format(i)
        startIndex = imageStartIndex + i * (workloadPerDirectory + 1)
        endIndex = imageStartIndex + (i + 1) * (workloadPerDirectory + 1)
        if (i >= workloadMore):
            startIndex = imageStartIndex + i * workloadPerDirectory + workloadMore
            endIndex = imageStartIndex + (i + 1) * workloadPerDirectory  + workloadMore
        pool.apply_async(postprocess, (startIndex, endIndex, dirName, ))  
    pool.close()
    pool.join()

    packTextures(imageStartIndex, imageEndIndex)

    # clean invalid files
    cleaning(imageStartIndex, imageEndIndex)
    for i in range(0, directoryNumber):
        dirName = formattedDirectoryName.format(i)
        shutil.rmtree(dirName)

    print("\n...DONE...\n")
