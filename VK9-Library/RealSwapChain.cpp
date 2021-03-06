// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

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

#include "RealSwapChain.h"

#include "Utilities.h"

RealSwapChain::RealSwapChain(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, HWND windowHandle, uint32_t width, uint32_t height, bool useVsync)
	: mInstance(instance),
	mPhysicalDevice(physicalDevice),
	mDevice(device),
	mWindowHandle(windowHandle),
	mWidth(width),
	mHeight(height),
	mUseVsync(useVsync)
{
	Log(info) << "RealSwapChain::RealSwapChain" << std::endl;

	mCommandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	
	vk::SemaphoreCreateInfo semaphoreInfo;

	vk::FenceCreateInfo fenceInfo;
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	InitSurface();
	InitSwapChain();

	mImageAvailableSemaphores.resize(mSwapchainImageCount);
	mRenderFinishedSemaphores.resize(mSwapchainImageCount);
	mInFlightFences.resize(mSwapchainImageCount);

	for (size_t i = 0; i < mSwapchainImageCount; i++)
	{
		if (mDevice.createSemaphore(&semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != vk::Result::eSuccess
			|| mDevice.createSemaphore(&semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != vk::Result::eSuccess
			|| mDevice.createFence(&fenceInfo, nullptr, &mInFlightFences[i]) != vk::Result::eSuccess)
		{
			Log(fatal) << "RealSwapChain::RealSwapChain failed to create synchronization objects for a frame!" << std::endl;
			return;
		}
	}

	InitDepthBuffer(); //Might not need will revisit later.

	//mImageMemoryBarrier.srcAccessMask = 0;
	mImageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead; //VK_ACCESS_MEMORY_READ_BIT;
	mImageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
	mImageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR; //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	mImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.image = mImages[mCurrentImageIndex];
	mImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	mPrePresentBarrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead; //VK_ACCESS_MEMORY_READ_BIT;
	mPrePresentBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead; //VK_ACCESS_MEMORY_READ_BIT;
	mPrePresentBarrier.oldLayout = vk::ImageLayout::ePresentSrcKHR; //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	mPrePresentBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR; //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	mPrePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mPrePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mPrePresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	mPipeStageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	//mSubmitInfo.waitSemaphoreCount = 1;
	mSubmitInfo.waitSemaphoreCount = 0;
	//mSubmitInfo.pWaitSemaphores = &mSwapSemaphore;
	mSubmitInfo.pWaitSemaphores = nullptr;
	mSubmitInfo.pWaitDstStageMask = &mPipeStageFlags;
	mSubmitInfo.commandBufferCount = 1;
	//mSubmitInfo.pCommandBuffers = &commandBuffer;
	mSubmitInfo.signalSemaphoreCount = 1;
	//mSubmitInfo.pSignalSemaphores = &mSwapSemaphore;
}

RealSwapChain::~RealSwapChain()
{
	Log(info) << "RealSwapChain::~RealSwapChain" << std::endl;

	DestroyDepthBuffer(); //Might not need will revisit later.
	DestroySwapChain();
	DestroySurface();

	for (size_t i = 0; i < mSwapchainImageCount; i++)
	{
		mDevice.destroySemaphore(mRenderFinishedSemaphores[i], nullptr);
		mDevice.destroySemaphore(mImageAvailableSemaphores[i], nullptr);
		mDevice.destroyFence(mInFlightFences[i], nullptr);
	}
}

void RealSwapChain::InitSurface()
{
	//Create Win32Surcace which is the drawable area of a win32 window. 
	vk::Win32SurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
	surfaceCreateInfo.hwnd = mWindowHandle;

	mResult = mInstance.createWin32SurfaceKHR(&surfaceCreateInfo, nullptr, &mSurface);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSurface vkCreateWin32SurfaceKHR failed with a return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	mResult = mPhysicalDevice.getSurfaceCapabilitiesKHR(mSurface, &mSurfaceCapabilities);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSurface vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	mSwapchainExtent = mSurfaceCapabilities.currentExtent;

	//Find surface formats
	mResult = mPhysicalDevice.getSurfaceFormatsKHR(mSurface, &mSurfaceFormatCount, nullptr);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSurface vkGetPhysicalDeviceSurfaceFormatsKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	mSurfaceFormats = new vk::SurfaceFormatKHR[mSurfaceFormatCount];
	mResult = mPhysicalDevice.getSurfaceFormatsKHR(mSurface, &mSurfaceFormatCount, mSurfaceFormats);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSurface vkGetPhysicalDeviceSurfaceFormatsKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	if (mSurfaceFormatCount == 1 && mSurfaceFormats[0].format == vk::Format::eUndefined)
	{
		mSurfaceFormat = vk::Format::eB8G8R8A8Unorm; //No preferred format so set a default.
	}
	else
	{
		mSurfaceFormat = mSurfaceFormats[0].format; //Pull the preferred format.
	}

	if (mSurfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
	{
		mTransformFlags = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	}
	else
	{
		mTransformFlags = mSurfaceCapabilities.currentTransform;
	}

	//Search for queues to use for presentation.
	uint32_t queueFamilyPropertyCount;
	mPhysicalDevice.getQueueFamilyProperties(&queueFamilyPropertyCount, nullptr);
	auto queueFamilyProperties = new vk::QueueFamilyProperties[queueFamilyPropertyCount];
	mPhysicalDevice.getQueueFamilyProperties(&queueFamilyPropertyCount, queueFamilyProperties);

	bool doesSupportPresentation = false;
	uint32_t presentationQueueIndex = 0;
	for (uint32_t i = 0; i < queueFamilyPropertyCount; i++)
	{
		auto result = mPhysicalDevice.getSurfaceSupportKHR(i, mSurface);
		if (result.result == vk::Result::eSuccess && result.value)
		{
			presentationQueueIndex = i;
			break;
		}
	}

	delete[] queueFamilyProperties;
	queueFamilyProperties = nullptr;

	/*
	Trying modes in order of preference (Mailbox,immediate,FIFO)
	VK_PRESENT_MODE_MAILBOX_KHR - Wait for the next vertical blanking interval to update the image. New images replace the one waiting to be displayed.
	VK_PRESENT_MODE_IMMEDIATE_KHR - Do not wait for vertical blanking to update the image.
	VK_PRESENT_MODE_FIFO_KHR - Wait for the next vertical blanking interval to update the image. If the interval is missed wait for the next one. New images will be queued for display.
	*/
	uint32_t presentationModeCount;
	mResult = mPhysicalDevice.getSurfacePresentModesKHR(mSurface, &presentationModeCount, nullptr);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSurface vkGetPhysicalDeviceSurfacePresentModesKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	auto presentationModes = new vk::PresentModeKHR[presentationModeCount];
	mResult = mPhysicalDevice.getSurfacePresentModesKHR(mSurface, &presentationModeCount, presentationModes);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSurface vkGetPhysicalDeviceSurfacePresentModesKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	mPresentationMode = vk::PresentModeKHR::eFifo;
	if (!mUseVsync)
	{
		for (size_t i = 0; i < presentationModeCount; i++)
		{
			if (presentationModes[i] == vk::PresentModeKHR::eMailbox)
			{
				mPresentationMode = vk::PresentModeKHR::eMailbox;
				break;
			}
			else if (presentationModes[i] == vk::PresentModeKHR::eImmediate)
			{
				mPresentationMode = vk::PresentModeKHR::eImmediate;
			}
		}
	}
	delete[] presentationModes;
}

void RealSwapChain::DestroySurface()
{
	mInstance.destroySurfaceKHR(mSurface, nullptr);
	delete[] mSurfaceFormats;
}

void RealSwapChain::InitSwapChain()
{
	/*
	Finally create the swap chain based on the information collected.
	This swap chain will handle the work done by the implicit swap chain in D3D9.
	*/
	vk::SwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.surface = mSurface;
	swapchainCreateInfo.minImageCount = mSurfaceCapabilities.minImageCount + 1;
	swapchainCreateInfo.imageFormat = mSurfaceFormat;
	swapchainCreateInfo.imageColorSpace = mSurfaceFormats[0].colorSpace;
	swapchainCreateInfo.imageExtent = mSwapchainExtent;
	swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
	swapchainCreateInfo.preTransform = mTransformFlags;
	swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.presentMode = mPresentationMode;
	swapchainCreateInfo.oldSwapchain = mSwapchain; //There is no old swapchain yet.
	swapchainCreateInfo.clipped = true;

	mResult = mDevice.createSwapchainKHR(&swapchainCreateInfo, nullptr, &mSwapchain);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSwapChain vkCreateSwapchainKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	//Create the images (buffers) that will be used by the swap chain.
	mResult = mDevice.getSwapchainImagesKHR(mSwapchain, &mSwapchainImageCount, nullptr);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSwapChain vkGetSwapchainImagesKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	mImages = new vk::Image[mSwapchainImageCount];
	mViews = new vk::ImageView[mSwapchainImageCount];

	mResult = mDevice.getSwapchainImagesKHR(mSwapchain, &mSwapchainImageCount, mImages);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitSwapChain vkGetSwapchainImagesKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	for (size_t i = 0; i < mSwapchainImageCount; i++)
	{
		vk::ImageViewCreateInfo color_image_view;
		color_image_view.format = mSurfaceFormat;
		color_image_view.components.r = vk::ComponentSwizzle::eIdentity;
		color_image_view.components.g = vk::ComponentSwizzle::eIdentity;
		color_image_view.components.b = vk::ComponentSwizzle::eIdentity;
		color_image_view.components.a = vk::ComponentSwizzle::eOne; //Should the window background ever be transparent?
		color_image_view.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		color_image_view.subresourceRange.baseMipLevel = 0;
		color_image_view.subresourceRange.levelCount = 1;
		color_image_view.subresourceRange.baseArrayLayer = 0;
		color_image_view.subresourceRange.layerCount = 1;
		color_image_view.viewType = vk::ImageViewType::e2D;
		color_image_view.image = mImages[i];

		mResult = mDevice.createImageView(&color_image_view, nullptr, &mViews[i]);
		if (mResult != vk::Result::eSuccess)
		{
			Log(fatal) << "RealSwapChain::InitSwapChain vkCreateImageView failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
			return;
		}
	}

	mPresentInfo.swapchainCount = 1;
	mPresentInfo.pSwapchains = &mSwapchain;
	mPresentInfo.waitSemaphoreCount = 1;
	//mPresentInfo.waitSemaphoreCount = 0;
	//mPresentInfo.pWaitSemaphores = &mSwapSemaphore;
	//mPresentInfo.pWaitSemaphores = nullptr;
}

void RealSwapChain::DestroySwapChain()
{
	mDevice.destroySwapchainKHR(mSwapchain, nullptr);

	if (mViews != nullptr)
	{
		for (size_t i = 0; i < mSwapchainImageCount; i++)
		{
			if (mViews[i] != vk::ImageView())
			{
				mDevice.destroyImageView(mViews[i], nullptr);
			}
		}
		delete[] mViews;
	}

	//if (mImages != nullptr)
	//{
	//For some reason destroying the images causes a crash. I'm guessing it's a double free or something like that because the views have already been destroyed.
	//for (int32_t i = 0; i < mSwapchainImageCount; i++)
	//{
	//	if (mImages[i] != VK_NULL_HANDLE)
	//	{
	//		vkDestroyImage(mDevice, mImages[i], nullptr);
	//	}	
	//}
	delete[] mImages;
	//}
}

void RealSwapChain::InitDepthBuffer()
{
	vk::ImageCreateInfo imageCreateInfo;
	imageCreateInfo.imageType = vk::ImageType::e2D;
	imageCreateInfo.format = mDepthFormat;
	imageCreateInfo.extent = vk::Extent3D(mWidth, mHeight, 1);
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
	imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
	imageCreateInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst;

	mResult = mDevice.createImage(&imageCreateInfo, nullptr, &mDepthImage);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitDepthBuffer vkCreateImage failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	vk::MemoryRequirements memoryRequirements;
	mDevice.getImageMemoryRequirements(mDepthImage, &memoryRequirements);

	vk::MemoryAllocateInfo depthMemoryAllocateInfo;
	depthMemoryAllocateInfo.memoryTypeIndex = 0;
	depthMemoryAllocateInfo.allocationSize = memoryRequirements.size;

	vk::PhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	mPhysicalDevice.getMemoryProperties(&physicalDeviceMemoryProperties);

	GetMemoryTypeFromProperties(physicalDeviceMemoryProperties, memoryRequirements.memoryTypeBits, 0, &depthMemoryAllocateInfo.memoryTypeIndex);

	mResult = mDevice.allocateMemory(&depthMemoryAllocateInfo, nullptr, &mDepthDeviceMemory);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitDepthBuffer vkAllocateMemory failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}

	mDevice.bindImageMemory(mDepthImage, mDepthDeviceMemory, 0);

	//ptr->SetImageLayout(mDepthImage, vk::ImageAspectFlagBits::eDepth, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

	vk::ImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.format = mDepthFormat;
	imageViewCreateInfo.subresourceRange = vk::ImageSubresourceRange();
	imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
	imageViewCreateInfo.image = mDepthImage;

	mResult = mDevice.createImageView(&imageViewCreateInfo, nullptr, &mDepthView);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::InitDepthBuffer vkCreateImageView failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return;
	}
}

void RealSwapChain::DestroyDepthBuffer()
{
	mDevice.destroyImageView(mDepthView, nullptr);
	mDevice.destroyImage(mDepthImage, nullptr);
	mDevice.freeMemory(mDepthDeviceMemory, nullptr);
}

vk::Result RealSwapChain::Present(vk::CommandBuffer& commandBuffer, vk::Queue& queue, vk::Image& source, int32_t width, int32_t height)
{
	mDevice.waitForFences(1, &mInFlightFences[mCurrentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	mDevice.resetFences(1, &mInFlightFences[mCurrentFrameIndex]);

	mResult = mDevice.acquireNextImageKHR(mSwapchain, std::numeric_limits<uint64_t>::max(), mImageAvailableSemaphores[mCurrentFrameIndex], nullptr, &mCurrentImageIndex);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::Start vkAcquireNextImageKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		commandBuffer.end();
		commandBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
		return mResult;
	}

	mImageMemoryBarrier.srcAccessMask = vk::AccessFlags(); //vk::AccessFlagBits::eMemoryWrite
	mImageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	mImageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
	mImageMemoryBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
	mImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.image = source;
	mImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &mImageMemoryBarrier);

	mImageMemoryBarrier.srcAccessMask = vk::AccessFlags();
	mImageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	mImageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
	mImageMemoryBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	mImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.image = mImages[mCurrentImageIndex];
	mImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &mImageMemoryBarrier);

	vk::ImageSubresourceLayers subResource1;
	subResource1.aspectMask = vk::ImageAspectFlagBits::eColor;
	subResource1.baseArrayLayer = 0;
	subResource1.mipLevel = 0;
	subResource1.layerCount = 1;

	vk::ImageSubresourceLayers subResource2;
	subResource2.aspectMask = vk::ImageAspectFlagBits::eColor;
	subResource2.baseArrayLayer = 0;
	subResource2.mipLevel = 0;
	subResource2.layerCount = 1;

	vk::ImageBlit region;
	region.srcSubresource = subResource1;
	region.dstSubresource = subResource2;
	region.srcOffsets[1] = vk::Offset3D(width, height, 1);
	region.dstOffsets[1] = vk::Offset3D(mSwapchainExtent.width, mSwapchainExtent.height, 1);

	commandBuffer.blitImage(
		source, vk::ImageLayout::eTransferSrcOptimal,
		mImages[mCurrentImageIndex], vk::ImageLayout::eTransferDstOptimal,
		1, &region, vk::Filter::eLinear);

	//vk::ImageCopy region;
	//region.srcSubresource = subResource1;
	//region.dstSubresource = subResource2;
	//region.srcOffset = vk::Offset3D(0, 0, 0);
	//region.dstOffset = vk::Offset3D(0, 0, 0);
	//region.extent.width = mSwapchainExtent.width;
	//region.extent.height = mSwapchainExtent.height;
	//region.extent.depth = 1;

	//commandBuffer.copyImage(
	//	source, vk::ImageLayout::eTransferSrcOptimal, /*eTransferSrcOptimal*/
	//	mImages[mCurrentIndex], vk::ImageLayout::eTransferDstOptimal,
	//	1, &region);

	mImageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	mImageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
	mImageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	mImageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	mImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	mImageMemoryBarrier.image = mImages[mCurrentImageIndex];
	mImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &mImageMemoryBarrier);

	//mImageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
	//mImageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	//mImageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
	//mImageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
	//mImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//mImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//mImageMemoryBarrier.image = source;
	//mImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	//commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &mImageMemoryBarrier);

	commandBuffer.end();

	mSubmitInfo.waitSemaphoreCount = 1;
	mSubmitInfo.pWaitSemaphores = &mImageAvailableSemaphores[mCurrentFrameIndex];
	mSubmitInfo.pWaitDstStageMask = mWaitStages;

	mSubmitInfo.commandBufferCount = 1;
	mSubmitInfo.pCommandBuffers = &commandBuffer;

	mSubmitInfo.signalSemaphoreCount = 1;
	mSubmitInfo.pSignalSemaphores = &mRenderFinishedSemaphores[mCurrentFrameIndex];

	mResult = queue.submit(1, &mSubmitInfo, mInFlightFences[mCurrentFrameIndex]);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::Present vkQueueSubmit failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return mResult;
	}

	//Using C API because someone decided to do an assert here in the C++ API instead of returning an error code.
	//mResult = (vk::Result)vkQueueWaitIdle((VkQueue)queue);
	//if (mResult != vk::Result::eSuccess)
	//{
	//	Log(fatal) << "RealSwapChain::Present vkQueueWaitIdle failed with return code of " << GetResultString((VkResult)mResult);
	//	return mResult;
	//}

	mPresentInfo.waitSemaphoreCount = 1;
	mPresentInfo.pWaitSemaphores = &mRenderFinishedSemaphores[mCurrentFrameIndex];
	mPresentInfo.pImageIndices = &mCurrentImageIndex;
	mResult = queue.presentKHR(&mPresentInfo);
	if (mResult != vk::Result::eSuccess)
	{
		Log(fatal) << "RealSwapChain::Present vkQueuePresentKHR failed with return code of " << GetResultString((VkResult)mResult) << std::endl;
		return mResult;
	}

	/*
	Unforunately I don't have a system for tracking resources used in a buffer and holding on to them until the buffer has executed so for now I need this heavy wait here.
	*/
	queue.waitIdle();
	//commandBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);

	mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mSwapchainImageCount;

	return mResult;
}