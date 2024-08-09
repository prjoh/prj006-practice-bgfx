#include <Loading.h>


#include <bimg/decode.h>

#include <iostream>


namespace zv
{
	bx::AllocatorI* LoadingManager::s_Allocator = NULL;
	bx::FileReaderI* LoadingManager::s_FileReader = NULL;
	bx::FileWriterI* LoadingManager::s_FileWriter = NULL;


	void LoadingManager::init()
	{
		s_FileReader = BX_NEW(getAllocator(), bx::FileReader);
		s_FileWriter = BX_NEW(getAllocator(), bx::FileWriter);
	}

	void LoadingManager::quit()
	{
		bx::deleteObject(s_Allocator, s_FileReader);
        s_FileReader = NULL;

		bx::deleteObject(s_Allocator, s_FileWriter);
        s_FileWriter = NULL;
	}

	void* LoadingManager::load(const char* _filePath, u32* _size)
	{
		return load(getFileReader(), getAllocator(), _filePath, _size);
	}

	void LoadingManager::unload(void* _ptr)
	{
		bx::free(getAllocator(), _ptr);
	}

	bgfx::TextureHandle LoadingManager::loadTexture(const char* _filePath, u64 _flags, u8 _skip, bgfx::TextureInfo* _info, bimg::Orientation::Enum* _orientation)
	{
		return loadTexture(getFileReader(), _filePath, _flags, _skip, _info, _orientation);
	}

	bgfx::ProgramHandle LoadingManager::loadProgram(const char* _vsPath, const char* _fsPath)
	{
		return loadProgram(getFileReader(), _vsPath, _fsPath);
	}


	bx::AllocatorI* LoadingManager::getDefaultAllocator()
	{
		BX_PRAGMA_DIAGNOSTIC_PUSH();
		BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4459); // warning C4459: declaration of 's_allocator' hides global declaration
		BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wshadow");
		static bx::DefaultAllocator s_allocator;
		return &s_allocator;
		BX_PRAGMA_DIAGNOSTIC_POP();
	}

    bx::AllocatorI* LoadingManager::getAllocator()
    {
        if (NULL == s_Allocator)
        {
            s_Allocator = getDefaultAllocator();
        }

        return s_Allocator;
    }

    bx::FileReaderI* LoadingManager::getFileReader()
    {
        return s_FileReader;
    }

    bx::FileWriterI* LoadingManager::getFileWriter()
    {
        return s_FileWriter;
    }

    void* LoadingManager::load(bx::FileReaderI* _reader, bx::AllocatorI* _allocator, const char* _filePath, u32* _size)
    {
        if (bx::open(_reader, _filePath))
        {
            u32 size = (u32)bx::getSize(_reader);
            void* data = bx::alloc(_allocator, size);
            bx::read(_reader, data, size, bx::ErrorAssert{});
            bx::close(_reader);
            if (NULL != _size)
            {
                *_size = size;
            }
            return data;
        }
        else
        {
            // TODO
            std::cout << "Failed to open: " << _filePath << "\n";
        }

        if (NULL != _size)
        {
            *_size = 0;
        }

        return NULL;
    }

    void LoadingManager::imageReleaseCb(void* _ptr, void* _userData)
    {
        BX_UNUSED(_ptr);
        bimg::ImageContainer* imageContainer = (bimg::ImageContainer*)_userData;
        bimg::imageFree(imageContainer);
    }

    bgfx::TextureHandle LoadingManager::loadTexture(bx::FileReaderI* _reader, const char* _filePath, u64 _flags, u8 _skip, bgfx::TextureInfo* _info, bimg::Orientation::Enum* _orientation)
    {
        BX_UNUSED(_skip);
        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;

        u32 size;
        void* data = load(_reader, getAllocator(), _filePath, &size);
        if (NULL != data)
        {
            bimg::ImageContainer* imageContainer = bimg::imageParse(getAllocator(), data, size);

            if (NULL != imageContainer)
            {
                if (NULL != _orientation)
                {
                    *_orientation = imageContainer->m_orientation;
                }

                const bgfx::Memory* mem = bgfx::makeRef(
                    imageContainer->m_data
                    , imageContainer->m_size
                    , imageReleaseCb
                    , imageContainer
                );
                unload(data);

                if (imageContainer->m_cubeMap)
                {
                    handle = bgfx::createTextureCube(
                        u16(imageContainer->m_width)
                        , 1 < imageContainer->m_numMips
                        , imageContainer->m_numLayers
                        , bgfx::TextureFormat::Enum(imageContainer->m_format)
                        , _flags
                        , mem
                    );
                }
                else if (1 < imageContainer->m_depth)
                {
                    handle = bgfx::createTexture3D(
                        u16(imageContainer->m_width)
                        , u16(imageContainer->m_height)
                        , u16(imageContainer->m_depth)
                        , 1 < imageContainer->m_numMips
                        , bgfx::TextureFormat::Enum(imageContainer->m_format)
                        , _flags
                        , mem
                    );
                }
                else if (bgfx::isTextureValid(0, false, imageContainer->m_numLayers, bgfx::TextureFormat::Enum(imageContainer->m_format), _flags))
                {
                    handle = bgfx::createTexture2D(
                        u16(imageContainer->m_width)
                        , u16(imageContainer->m_height)
                        , 1 < imageContainer->m_numMips
                        , imageContainer->m_numLayers
                        , bgfx::TextureFormat::Enum(imageContainer->m_format)
                        , _flags
                        , mem
                    );
                }

                if (bgfx::isValid(handle))
                {
                    bgfx::setName(handle, _filePath);
                }

                if (NULL != _info)
                {
                    bgfx::calcTextureSize(
                        *_info
                        , u16(imageContainer->m_width)
                        , u16(imageContainer->m_height)
                        , u16(imageContainer->m_depth)
                        , imageContainer->m_cubeMap
                        , 1 < imageContainer->m_numMips
                        , imageContainer->m_numLayers
                        , bgfx::TextureFormat::Enum(imageContainer->m_format)
                    );
                }
            }
        }

        return handle;
    }

    const bgfx::Memory* LoadingManager::loadMem(bx::FileReaderI* _reader, const char* _filePath)
    {
        if (bx::open(_reader, _filePath))
        {
            u32 size = (u32)bx::getSize(_reader);
            const bgfx::Memory* mem = bgfx::alloc(size + 1);
            bx::read(_reader, mem->data, size, bx::ErrorAssert{});
            bx::close(_reader);
            mem->data[mem->size - 1] = '\0';
            return mem;
        }

        // TODO
        std::cout << "Failed to load: " << _filePath;
        return NULL;
    }

    bgfx::ShaderHandle LoadingManager::loadShader(bx::FileReaderI* _reader, const char* _path)
    {
        bgfx::ShaderHandle handle = bgfx::createShader(loadMem(_reader, _path));
        // TODO
        //bgfx::setName(handle, _name);
        return handle;
    }

    bgfx::ProgramHandle LoadingManager::loadProgram(bx::FileReaderI* _reader, const char* _vsPath, const char* _fsPath)
    {
        bgfx::ShaderHandle vsh = loadShader(_reader, _vsPath);
        bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
        if (NULL != _fsPath)
        {
            fsh = loadShader(_reader, _fsPath);
        }

        return bgfx::createProgram(vsh, fsh, true /* destroy shaders when program is destroyed */);
    }
}
