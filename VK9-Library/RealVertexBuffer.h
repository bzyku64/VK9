/*
Copyright(c) 2018 Christopher Joseph Dean Schaefer

This software is provided 'as-is', without any express or implied
warranty.In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software.If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include <vulkan/vulkan.hpp>
#include <vulkan/vk_sdk_platform.h>

#include "RealDevice.h"

#ifndef REALVERTEXBUFFER_H
#define REALVERTEXBUFFER_H

struct RealVertexBuffer
{
	VmaAllocationInfo mAllocationInfo = {};
	
	VmaAllocation mAllocation = {};
	vk::Buffer mBuffer;

	void* mPersistentData = nullptr;
	void* mData = nullptr;
	int32_t mSize;
	size_t mLength;
	bool mIsDynamic;

	RealDevice* mRealDevice = nullptr; //null if not owner.
	RealVertexBuffer(RealDevice* realDevice, size_t length, bool isDynamic, DWORD fvf);
	~RealVertexBuffer();

	void* Lock(size_t offset);
	void Unlock();

};

#endif // REALVERTEXBUFFER_H