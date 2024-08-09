#pragma once


#include <bgfx/bgfx.h>
#include <bx/file.h>
#include <bx/pixelformat.h>
#include <bimg/bimg.h>

#include <Types.h>


namespace zv
{
	class LoadingManager
	{
	private:
		LoadingManager() = default;

	public:
		static void init();
		static void quit();

		static void* load(const char* _filePath, u32* _size = NULL);
		static void unload(void* _ptr);

		static bgfx::TextureHandle loadTexture(const char* _filePath,
												u64 _flags = 0x00, 
												u8 _skip = 0, 
												bgfx::TextureInfo* _info = NULL, 
												bimg::Orientation::Enum* _orientation = NULL);
		static bgfx::ProgramHandle loadProgram(const char* _vsPath, const char* _fsPath);

	private:
		static bx::AllocatorI* getDefaultAllocator();
		static bx::AllocatorI* LoadingManager::getAllocator();
		static bx::FileReaderI* LoadingManager::getFileReader();
		static bx::FileWriterI* LoadingManager::getFileWriter();
		static void* LoadingManager::load(bx::FileReaderI* _reader, bx::AllocatorI* _allocator, const char* _filePath, u32* _size);
		static void LoadingManager::imageReleaseCb(void* _ptr, void* _userData);
		static bgfx::TextureHandle LoadingManager::loadTexture(bx::FileReaderI* _reader,
																const char* _filePath, 
																u64 _flags, 
																u8 _skip, 
																bgfx::TextureInfo* _info, 
																bimg::Orientation::Enum* _orientation);
		static const bgfx::Memory* LoadingManager::loadMem(bx::FileReaderI* _reader, const char* _filePath);
		static bgfx::ShaderHandle LoadingManager::loadShader(bx::FileReaderI* _reader, const char* _path);
		static bgfx::ProgramHandle LoadingManager::loadProgram(bx::FileReaderI* _reader, const char* _vsPath, const char* _fsPath);

	private:
		static bx::FileReaderI* s_FileReader;
		static bx::FileWriterI* s_FileWriter;
		static bx::AllocatorI* s_Allocator;
	};
}