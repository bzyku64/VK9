/*
Copyright(c) 2016 Christopher Joseph Dean Schaefer

This software is provided 'as-is', without any express or implied
warranty.In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software.If you use this software
in a product, an acknowledgement in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "C9.h"
#include "CDevice9.h"
#include "CCubeTexture9.h"
#include "CBaseTexture9.h"
#include "CTexture9.h"
#include "CVolumeTexture9.h"
#include "CSurface9.h"
#include "CVertexDeclaration9.h"
#include "CVertexShader9.h"

#include "Utilities.h"

CDevice9::CDevice9(C9* Instance, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters)
	: mInstance(Instance),
	mAdapter(Adapter),
	mDeviceType(DeviceType),
	mFocusWindow(hFocusWindow),
	mBehaviorFlags(BehaviorFlags),
	mPresentationParameters(pPresentationParameters),
	mQueueCount(0),
	mReferenceCount(0),
	mDisplays(NULL),
	mDisplayCount(0),
	mGraphicsQueueIndex(UINT32_MAX),
	mPresentationQueueIndex(UINT32_MAX),
	mSurfaceFormatCount(0),
	mSurfaceFormats(NULL),
	mSwapchainPresentMode(VK_PRESENT_MODE_FIFO_KHR),
	mPresentationModeCount(0),
	mPresentationModes(NULL),
	mSwapchainImages(NULL),
	mSwapchainBuffers(NULL),
	mSwapchainViews(NULL),
	mSwapchainImageCount(0),
	mCommandBuffer(VK_NULL_HANDLE),
	mNullFence(VK_NULL_HANDLE),
	mFramebuffers(NULL)
{
	mPhysicalDevice = mInstance->mPhysicalDevices[mAdapter]; //pull the selected physical device from the instance.

	//Fetch the queue properties.
	vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &mQueueCount,NULL);
	mQueueFamilyProperties = new VkQueueFamilyProperties[mQueueCount];
	vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &mQueueCount,mQueueFamilyProperties);

	/*bool found = false;
	for (unsigned int i = 0; i < mQueueCount; i++) 
	{
		if (mQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
		{
			queue_info.queueFamilyIndex = i;
			found = true;
			break;
		}
	}*/

	mDeviceExtensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	float queue_priorities[1] = { 0.0 };
	VkDeviceQueueCreateInfo queue_info = {};	
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.pNext = NULL;
	queue_info.queueCount = 1;
	queue_info.pQueuePriorities = queue_priorities;

	VkDeviceCreateInfo device_info = {};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pNext = NULL;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos = &queue_info;
	device_info.enabledExtensionCount = mDeviceExtensionNames.size();
	device_info.ppEnabledExtensionNames = mDeviceExtensionNames.data();
	device_info.enabledLayerCount = 0;
	device_info.ppEnabledLayerNames = NULL;
	device_info.pEnabledFeatures = NULL;

	vkCreateDevice(mPhysicalDevice, &device_info, NULL, &mDevice);

	/*
	Now that the rendering is setup the surface must be created.
	The surface maybe inside of a window or a whole display. (Think SDL)
	*/
	if (!mPresentationParameters->Windowed)
	{
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};

		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = NULL;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.hwnd = hFocusWindow;
		surfaceCreateInfo.hinstance = GetModuleHandle(NULL);

		vkCreateWin32SurfaceKHR(mInstance->mInstance, &surfaceCreateInfo, NULL, &mSurface);
	}
	else
	{
		//TODO: finish full screen support.
		/*vkGetDisplayPlaneSupportedDisplaysKHR(mPhysicalDevice, 0, &mDisplayCount, NULL);
		mDisplays = new VkDisplayKHR[mDisplayCount];
		vkGetDisplayPlaneSupportedDisplaysKHR(mPhysicalDevice, 0, &mDisplayCount, mDisplays);

		//vkGetDisplayModePropertiesKHR(mPhysicalDevice,mDisplays[0])

		VkDisplaySurfaceCreateInfoKHR surfaceCreateInfo = {};

		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = NULL;
		surfaceCreateInfo.flags = 0;



		vkCreateDisplayPlaneSurfaceKHR(mInstance->mInstance,&surfaceCreateInfo,NULL, &mSurface);*/
	}

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &mSurfaceCapabilities);
	
	/*
	Search for queues to use for graphics and presentation.
	It's easier if one queue does both so if we find one that supports both than just exit.
	Otherwise look for one for presentation and one for graphics.
	The index of the queue us stored for later use.
	*/
	for (size_t i = 0; i < mQueueCount; i++)
	{
		VkBool32 doesSupportPresentation = false;
		VkBool32 doesSupportGraphics = false;

		vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &doesSupportPresentation);
		doesSupportGraphics = ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0);

		if (doesSupportPresentation && doesSupportGraphics)
		{
			mGraphicsQueueIndex = i;
			mPresentationQueueIndex = i;
			break;
		}
		else if (doesSupportPresentation && mPresentationQueueIndex == UINT32_MAX)
		{
			mPresentationQueueIndex = i;
		}
		else if (doesSupportGraphics && mGraphicsQueueIndex == UINT32_MAX)
		{
			mGraphicsQueueIndex = i;
		}

	}

	/*
	Now we need to setup our queues and buffers.
	We'll start with a command pool because that is where we get command buffers from.
	*/

	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = NULL;
	commandPoolInfo.queueFamilyIndex = mGraphicsQueueIndex; //Found earlier.
	commandPoolInfo.flags = 0;

	vkCreateCommandPool(mDevice, &commandPoolInfo, NULL, &mCommandPool);

	//Create queue so we can submit command buffers.
	vkGetDeviceQueue(mDevice, mGraphicsQueueIndex, 0,&mQueue);

	/*
	Now pull some information about the surface so we can create the swapchain correctly.
	*/

	if (mSurfaceCapabilities.currentExtent.width == (uint32_t)-1) 
	{
		//If the height/width are -1 then just set it to the requested size and hope for the best.
		mSwapchainExtent.width = mPresentationParameters->BackBufferWidth;
		mSwapchainExtent.height = mPresentationParameters->BackBufferHeight;
	}
	else 
	{
		//Appearently the swap chain size must match the surface size if it is defined.
		mSwapchainExtent = mSurfaceCapabilities.currentExtent;
	}

	vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &mSurfaceFormatCount, NULL);
	mSurfaceFormats = new VkSurfaceFormatKHR[mSurfaceFormatCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &mSurfaceFormatCount, mSurfaceFormats);

	if (mSurfaceFormatCount == 1 && mSurfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		mFormat = VK_FORMAT_B8G8R8A8_UNORM; //No prefered format so set a default.
	}
	else
	{
		mFormat = mSurfaceFormats[0].format; //Pull the prefered format.
	}

	if (mSurfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		mTransformFlags = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else 
	{
		mTransformFlags = mSurfaceCapabilities.currentTransform;
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &mPresentationModeCount, NULL);
	mPresentationModes = new VkPresentModeKHR[mPresentationModeCount];
	vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &mPresentationModeCount, mPresentationModes);

	/*
	Trying modes in order of preference (Mailbox,immediate, FIFO)
	VK_PRESENT_MODE_MAILBOX_KHR - Wait for the next vertical blanking interval to update the image. New images replace the one waiting to be displayed.
	VK_PRESENT_MODE_IMMEDIATE_KHR - Do not wait for vertical blanking to update the image.
	VK_PRESENT_MODE_FIFO_KHR - Wait for hte next virtical blanking interval to update the image. If the interval is missed wait for hte next one. New images will be queued for display.
	*/
	for (size_t i = 0; i < mPresentationModeCount; i++)
	{
		if (mPresentationModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			mSwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
		else if (mPresentationModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			mSwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		} //Already defaulted to FIFO so do nothing for else.
	}

	/*
	Finally create the swap chain based on the information collected.
	This swap chain will handle the work done by the implicit swap chain in D3D9.
	*/
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};

	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = NULL;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = mSurface;
	swapchainCreateInfo.minImageCount = mSurfaceCapabilities.minImageCount + 1;
	swapchainCreateInfo.imageFormat = ConvertFormat(mPresentationParameters->BackBufferFormat);
	swapchainCreateInfo.imageColorSpace  = mSurfaceFormats[0].colorSpace;
	swapchainCreateInfo.imageExtent = mSwapchainExtent;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = mTransformFlags;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = NULL;
	swapchainCreateInfo.presentMode = mSwapchainPresentMode;
	swapchainCreateInfo.oldSwapchain = mSwapchain; //There is no old swapchain yet.
	swapchainCreateInfo.clipped = true;

	vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, NULL, &mSwapchain);	

	//Create the images (buffers) that will be used by the swap chain.
	vkGetSwapchainImagesKHR(mDevice, mSurface, &mSwapchainImageCount, NULL);
	mSwapchainImages = new VkImage[mSwapchainImageCount];
	vkGetSwapchainImagesKHR(mDevice, mSurface, &mSwapchainImageCount, mSwapchainImages);

	for (size_t i = 0; i < mSwapchainImageCount; i++)
	{
		VkImageViewCreateInfo color_image_view = {};

		color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		color_image_view.pNext = NULL;
		color_image_view.format = mFormat;
		color_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
		color_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
		color_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
		color_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
		color_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		color_image_view.subresourceRange.baseMipLevel = 0;
		color_image_view.subresourceRange.levelCount = 1;
		color_image_view.subresourceRange.baseArrayLayer = 0;
		color_image_view.subresourceRange.layerCount = 1;
		color_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		color_image_view.flags = 0;

		this->SetImageLayout(mSwapchainImages[i], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		color_image_view.image = mSwapchainImages[i];	

		vkCreateImageView(mDevice, &color_image_view, NULL, &mSwapchainViews[i]);
	}

	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.pNext = NULL;
	commandBufferInfo.commandPool = mCommandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = 1;

	for (size_t i = 0; i < mSwapchainImageCount; i++)
	{
		vkAllocateCommandBuffers(mDevice, &commandBufferInfo, &mSwapchainBuffers[i]);
	}

	/*
	Now setup the render pass.
	*/

	VkAttachmentDescription renderAttachments[2];
	renderAttachments[0].format = mFormat;
	renderAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	renderAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	renderAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	renderAttachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	renderAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	renderAttachments[1].format = mFormat;  //demo->depth.format; (revsit)
	renderAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	renderAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	renderAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	renderAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	renderAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	renderAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.flags = 0;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = &depthReference;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pNext = NULL;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = renderAttachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 0;
	renderPassCreateInfo.pDependencies = NULL;

	vkCreateRenderPass(mDevice, &renderPassCreateInfo, NULL, &mRenderPass);

	/*
	Setup framebuffers to tie everything together.
	*/

	VkImageView attachments[1];
	//attachments[1] = demo->depth.view; //revsit

	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.pNext = NULL;
	framebufferCreateInfo.renderPass = mRenderPass;
	framebufferCreateInfo.attachmentCount = 1; //2 revisit
	framebufferCreateInfo.pAttachments = attachments;
	framebufferCreateInfo.width = mSwapchainExtent.width; //revisit
	framebufferCreateInfo.height = mSwapchainExtent.height; //revisit
	framebufferCreateInfo.layers = 1;

	mFramebuffers = new VkFramebuffer[mSwapchainImageCount];

	for (size_t i = 0; i < mSwapchainImageCount; i++)
	{
		attachments[0] = mSwapchainViews[i];
		vkCreateFramebuffer(mDevice, &framebufferCreateInfo, NULL, &mFramebuffers[i]);
	}
}

CDevice9::~CDevice9()
{
	if (mFramebuffers!=NULL)
	{
		for (size_t i = 0; i < mSwapchainImageCount; i++)
		{
			vkDestroyFramebuffer(mDevice, mFramebuffers[i], NULL);
		}
		delete[] mFramebuffers;
	}

	vkDestroyRenderPass(mDevice, mRenderPass, NULL);

	if (mSwapchainBuffers!=NULL)
	{
		vkFreeCommandBuffers(mDevice, mCommandPool, mSwapchainImageCount, mSwapchainBuffers);
		delete[] mSwapchainBuffers;
	}

	vkDestroyCommandPool(mDevice, mCommandPool, NULL);

	vkDestroySwapchainKHR(mDevice, mSwapchain, NULL);
	if (mSwapchainImages != NULL)
	{
		delete[] mSwapchainImages;
	}

	vkDestroyDevice(mDevice, NULL);

	vkDestroySurfaceKHR(mInstance->mInstance, mSurface, NULL);
	if (mPresentationModes != NULL)
	{
		delete[] mPresentationModes;
	}
	if (mSurfaceFormats != NULL)
	{
		delete[] mSurfaceFormats;
	}
	if (mDisplays != NULL)
	{
		delete[] mDisplays;
	}
}

HRESULT STDMETHODCALLTYPE CDevice9::Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::BeginScene()
{
	VkResult result = VK_SUCCESS;
	VkSemaphoreCreateInfo presentCompleteSemaphoreCreateInfo;
	
	presentCompleteSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	presentCompleteSemaphoreCreateInfo.pNext = NULL;
	presentCompleteSemaphoreCreateInfo.flags = 0;

	result = vkCreateSemaphore(mDevice, &presentCompleteSemaphoreCreateInfo, NULL, &mPresentCompleteSemaphore);

	if (result != VK_SUCCESS) return D3DERR_DEVICEREMOVED;

	result = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, mPresentCompleteSemaphore, (VkFence)0, &mCurrentBuffer);

	if (result != VK_SUCCESS) return D3DERR_DEVICEREMOVED;

	//SetImageLayout(mSwapchainImages[mCurrentBuffer], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// demo_flush_init_cmd

	VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
	commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	commandBufferInheritanceInfo.pNext = NULL;
	commandBufferInheritanceInfo.renderPass = VK_NULL_HANDLE;
	commandBufferInheritanceInfo.subpass = 0;
	commandBufferInheritanceInfo.framebuffer = VK_NULL_HANDLE;
	commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE;
	commandBufferInheritanceInfo.queryFlags = 0;
	commandBufferInheritanceInfo.pipelineStatistics = 0;

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = NULL;
	commandBufferBeginInfo.flags = 0;
	commandBufferBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

	VkClearValue clearValues[2];
	clearValues[0].color.float32[0] = 0.2f;
	clearValues[0].color.float32[1] = 0.2f;
	clearValues[0].color.float32[2] = 0.2f;
	clearValues[0].color.float32[3] = 0.2f;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = mRenderPass;
	renderPassBeginInfo.framebuffer = mFramebuffers[mCurrentBuffer];
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = mSwapchainExtent.width;
	renderPassBeginInfo.renderArea.extent.height = mSwapchainExtent.height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	result = vkBeginCommandBuffer(mSwapchainBuffers[mCurrentBuffer], &commandBufferBeginInfo);

	if (result != VK_SUCCESS) return D3DERR_DEVICEREMOVED;

	vkCmdBeginRenderPass(mSwapchainBuffers[mCurrentBuffer], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::EndScene()
{
	VkResult result = VK_SUCCESS;
	VkPipelineStageFlags pipeStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo submitInfo = {};

	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &mPresentCompleteSemaphore;
	submitInfo.pWaitDstStageMask = &pipeStageFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mSwapchainBuffers[mCurrentBuffer];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = NULL;

	result = vkEndCommandBuffer(mSwapchainBuffers[mCurrentBuffer]);

	if (result != VK_SUCCESS) return D3DERR_DEVICEREMOVED;

	result = vkQueueSubmit(mQueue, 1, &submitInfo, mNullFence);

	if (result != VK_SUCCESS) return D3DERR_DEVICEREMOVED;

	return D3D_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion)
{
	VkResult result = VK_SUCCESS;

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &mSwapchain;
	presentInfo.pImageIndices = &mCurrentBuffer;

	result = vkQueuePresentKHR(mQueue, &presentInfo);

	if (result == VK_SUCCESS) result = vkQueueWaitIdle(mQueue);

	vkDestroySemaphore(mDevice, mPresentCompleteSemaphore, NULL);

	if (result != VK_SUCCESS) return D3DERR_DEVICEREMOVED;

	return D3D_OK;
}

ULONG STDMETHODCALLTYPE CDevice9::AddRef(void)
{
	mReferenceCount++;

	return mReferenceCount;
}

HRESULT STDMETHODCALLTYPE CDevice9::QueryInterface(REFIID riid,void  **ppv)
{
	//TODO: Implement.

	return S_OK;
}

ULONG STDMETHODCALLTYPE CDevice9::Release(void)
{
	mReferenceCount--;

	if (mReferenceCount <= 0)
	{
		delete this;
	}

	return mReferenceCount;
}

	
HRESULT STDMETHODCALLTYPE CDevice9::BeginStateBlock()
{
	//TODO: Implement.

	return E_NOTIMPL;
}



HRESULT STDMETHODCALLTYPE CDevice9::ColorFill(IDirect3DSurface9 *pSurface,const RECT *pRect,D3DCOLOR color)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters,IDirect3DSwapChain9 **ppSwapChain)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9 **ppCubeTexture,HANDLE *pSharedHandle)
{
	HRESULT result = S_OK;

	IDirect3DCubeTexture9* obj = new CCubeTexture9(this, EdgeLength,Levels,Usage,Format,Pool,pSharedHandle);

	(*ppCubeTexture) = (IDirect3DCubeTexture9*)obj;

	return result;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9 **ppSurface,HANDLE *pSharedHandle)
{
	HRESULT result = S_OK;

	IDirect3DSurface9* obj = new CSurface9(this,Width,Height,Format,MultiSample,MultisampleQuality,Discard,pSharedHandle);

	(*ppSurface) = (IDirect3DSurface9*)obj;

	return result;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9 **ppIndexBuffer,HANDLE *pSharedHandle)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateOffscreenPlainSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9 **ppSurface,HANDLE *pSharedHandle)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreatePixelShader(const DWORD *pFunction,IDirect3DPixelShader9 **ppShader)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9 **ppQuery)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9 **ppSurface,HANDLE *pSharedHandle)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type,IDirect3DStateBlock9 **ppSB)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9 **ppTexture,HANDLE *pSharedHandle)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9 **ppVertexBuffer,HANDLE *pSharedHandle)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements,IDirect3DVertexDeclaration9 **ppDecl)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateVertexShader(const DWORD *pFunction,IDirect3DVertexShader9 **ppShader)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9 **ppVolumeTexture,HANDLE *pSharedHandle)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::DeletePatch(UINT Handle)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT PrimitiveCount)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,const void *pIndexData,D3DFORMAT IndexDataFormat,const void *pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,const void *pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::DrawRectPatch(UINT Handle,const float *pNumSegs,const D3DRECTPATCH_INFO *pRectPatchInfo)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::DrawTriPatch(UINT Handle,const float *pNumSegs,const D3DTRIPATCH_INFO *pTriPatchInfo)
{
	//TODO: Implement.

	return E_NOTIMPL;
}
	
HRESULT STDMETHODCALLTYPE CDevice9::EndStateBlock(IDirect3DStateBlock9 **ppSB)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::EvictManagedResources()
{
	//TODO: Implement.

	return S_OK;	
}

UINT STDMETHODCALLTYPE CDevice9::GetAvailableTextureMem()
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetBackBuffer(UINT  iSwapChain,UINT BackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9 **ppBackBuffer)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetClipPlane(DWORD Index,float *pPlane)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetClipStatus(D3DCLIPSTATUS9 *pClipStatus)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetCurrentTexturePalette(UINT *pPaletteNumber)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::GetDeviceCaps(D3DCAPS9 *pCaps)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::GetDirect3D(IDirect3D9 **ppD3D9)
{
	(*ppD3D9) = (IDirect3D9*)mInstance;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetDisplayMode(UINT  iSwapChain,D3DDISPLAYMODE *pMode)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetFrontBufferData(UINT  iSwapChain,IDirect3DSurface9 *pDestSurface)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::GetFVF(DWORD *pFVF)
{
	//TODO: Implement.

	return S_OK;	
}

void STDMETHODCALLTYPE CDevice9::GetGammaRamp(UINT  iSwapChain,D3DGAMMARAMP *pRamp)
{
	//TODO: Implement.

	return;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetIndices(IDirect3DIndexBuffer9 **ppIndexData) //,UINT *pBaseVertexIndex ?
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetLight(DWORD Index,D3DLIGHT9 *pLight)
{
	//TODO: Implement.

	return E_NOTIMPL;
} 

HRESULT STDMETHODCALLTYPE CDevice9::GetLightEnable(DWORD Index,BOOL *pEnable)
{
	//TODO: Implement.

	return S_OK;		
}

HRESULT STDMETHODCALLTYPE CDevice9::GetMaterial(D3DMATERIAL9 *pMaterial)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

FLOAT STDMETHODCALLTYPE CDevice9::GetNPatchMode()
{
	//TODO: Implement.

	return 0;
}

UINT STDMETHODCALLTYPE CDevice9::GetNumberOfSwapChains()
{
	//TODO: Implement.

	return 0;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY *pEntries)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetPixelShader(IDirect3DPixelShader9 **ppShader)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetPixelShaderConstantB(UINT StartRegister,BOOL *pConstantData,UINT BoolCount)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetPixelShaderConstantF(UINT StartRegister,float *pConstantData,UINT Vector4fCount)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetPixelShaderConstantI(UINT StartRegister,int *pConstantData,UINT Vector4iCount)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetRasterStatus(UINT  iSwapChain,D3DRASTER_STATUS *pRasterStatus)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetRenderState(D3DRENDERSTATETYPE State,DWORD *pValue)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9 **ppRenderTarget)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::GetRenderTargetData(IDirect3DSurface9 *pRenderTarget,IDirect3DSurface9 *pDestSurface)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::GetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD *pValue)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetScissorRect(RECT *pRect)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

BOOL STDMETHODCALLTYPE CDevice9::GetSoftwareVertexProcessing()
{
	//TODO: Implement.

	return true;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9 **ppStreamData,UINT *pOffsetInBytes,UINT *pStride)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetStreamSourceFreq(UINT StreamNumber,UINT *pDivider)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetSwapChain(UINT  iSwapChain,IDirect3DSwapChain9 **ppSwapChain)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetTexture(DWORD Stage,IDirect3DBaseTexture9 **ppTexture)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD *pValue)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX *pMatrix)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetVertexShader(IDirect3DVertexShader9 **ppShader)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetVertexShaderConstantB(UINT StartRegister,BOOL *pConstantData,UINT BoolCount)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetVertexShaderConstantF(UINT StartRegister,float *pConstantData,UINT Vector4fCount)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetVertexShaderConstantI(UINT StartRegister,int *pConstantData,UINT Vector4iCount)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::GetViewport(D3DVIEWPORT9 *pViewport)
{
	//TODO: Implement.

	return S_OK;		
}
	
HRESULT STDMETHODCALLTYPE CDevice9::LightEnable(DWORD LightIndex,BOOL bEnable)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State,const D3DMATRIX *pMatrix)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer9 *pDestBuffer,IDirect3DVertexDeclaration9 *pVertexDecl,DWORD Flags)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
{	
	//TODO: Implement.

	return S_OK;		
}

HRESULT CDevice9::SetClipPlane(DWORD Index,const float *pPlane)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetClipStatus(const D3DCLIPSTATUS9 *pClipStatus)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetCurrentTexturePalette(UINT PaletteNumber)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

void STDMETHODCALLTYPE CDevice9::SetCursorPosition(INT X,INT Y,DWORD Flags)
{
	//TODO: Implement.

	return;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface9 *pCursorBitmap)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetDialogBoxMode(BOOL bEnableDialogs)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetFVF(DWORD FVF)
{
	//TODO: Implement.

	return S_OK;	
}

void STDMETHODCALLTYPE CDevice9::SetGammaRamp(UINT  iSwapChain,DWORD Flags,const D3DGAMMARAMP *pRamp)
{
	//TODO: Implement.
}

HRESULT STDMETHODCALLTYPE CDevice9::SetIndices(IDirect3DIndexBuffer9 *pIndexData)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetLight(DWORD Index,const D3DLIGHT9 *pLight)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetMaterial(const D3DMATERIAL9 *pMaterial)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetNPatchMode(float nSegments)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetPaletteEntries(UINT PaletteNumber,const PALETTEENTRY *pEntries)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetPixelShader(IDirect3DPixelShader9 *pShader)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetPixelShaderConstantB(UINT StartRegister,const BOOL *pConstantData,UINT BoolCount)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetPixelShaderConstantF(UINT StartRegister,const float *pConstantData,UINT Vector4fCount)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetPixelShaderConstantI(UINT StartRegister,const int *pConstantData,UINT Vector4iCount)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9 *pRenderTarget)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetScissorRect(const RECT *pRect)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetSoftwareVertexProcessing(BOOL bSoftware)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9 *pStreamData,UINT OffsetInBytes,UINT Stride)
{
	//TODO: Implement.
		
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetStreamSourceFreq(UINT StreamNumber,UINT FrequencyParameter)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetTexture(DWORD Sampler,IDirect3DBaseTexture9 *pTexture)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetTransform(D3DTRANSFORMSTATETYPE State,const D3DMATRIX *pMatrix)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetVertexShader(IDirect3DVertexShader9 *pShader)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetVertexShaderConstantB(UINT StartRegister,const BOOL *pConstantData,UINT BoolCount)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetVertexShaderConstantF(UINT StartRegister,const float *pConstantData,UINT Vector4fCount)
{
	//TODO: Implement.

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDevice9::SetVertexShaderConstantI(UINT StartRegister,const int *pConstantData,UINT Vector4iCount)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::SetViewport(const D3DVIEWPORT9 *pViewport)
{
	//TODO: Implement.

	return S_OK;	
}

BOOL STDMETHODCALLTYPE CDevice9::ShowCursor(BOOL bShow)
{
	//TODO: Implement.

	return TRUE;	
}

HRESULT STDMETHODCALLTYPE CDevice9::StretchRect(IDirect3DSurface9 *pSourceSurface,const RECT *pSourceRect,IDirect3DSurface9 *pDestSurface,const RECT *pDestRect,D3DTEXTUREFILTERTYPE Filter)
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::TestCooperativeLevel()
{
	//TODO: Implement.

	return S_OK;	
}

HRESULT STDMETHODCALLTYPE CDevice9::UpdateSurface(IDirect3DSurface9 *pSourceSurface,const RECT *pSourceRect,IDirect3DSurface9 *pDestinationSurface,const POINT *pDestinationPoint)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture,IDirect3DBaseTexture9* pDestinationTexture)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDevice9::ValidateDevice(DWORD *pNumPasses)
{
	//TODO: Implement.

	return S_OK;	
}

void CDevice9::SetImageLayout(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout)
{
	/*
	This is just a helper method to reduce repeat code.
	*/
	VkResult result = VK_SUCCESS;
	VkPipelineStageFlags sourceStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destinationStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	// If the command buffer hasn't been created yet create it so it can be used.
	if (mCommandBuffer == VK_NULL_HANDLE)
	{
		VkCommandBufferAllocateInfo commandBufferInfo = {};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.pNext = NULL;
		commandBufferInfo.commandPool = mCommandPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferInfo.commandBufferCount = 1;

		result = vkAllocateCommandBuffers(mDevice, &commandBufferInfo, &mCommandBuffer);

		if (result != VK_SUCCESS) return;

		VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
		commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		commandBufferInheritanceInfo.pNext = NULL;
		commandBufferInheritanceInfo.renderPass = VK_NULL_HANDLE;
		commandBufferInheritanceInfo.subpass = 0;
		commandBufferInheritanceInfo.framebuffer = VK_NULL_HANDLE;
		commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE;
		commandBufferInheritanceInfo.queryFlags = 0;
		commandBufferInheritanceInfo.pipelineStatistics = 0;

		VkCommandBufferBeginInfo commandBufferBeginInfo;
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.pNext = NULL;
		commandBufferBeginInfo.flags = 0;
		commandBufferBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

		result = vkBeginCommandBuffer(mCommandBuffer, &commandBufferBeginInfo);

		if (result != VK_SUCCESS) return;
	}

	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = NULL;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = 0;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = { aspectMask, 0, 1, 0, 1 };

	if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	}

	if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	}

	vkCmdPipelineBarrier(mCommandBuffer, sourceStages, destinationStages, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
}