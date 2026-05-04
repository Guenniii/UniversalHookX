#define STB_IMAGE_IMPLEMENTATION
#include "../dependencies/images/stb_image.h"

#include "../dependencies/imgui/imgui_impl_vulkan.h"

#include <cstring>
#include <vulkan/vulkan.h>
#include "imageloader.hpp"
// Externe Vulkan Daten aus deinem Hook
extern VkDevice g_Device;
extern VkPhysicalDevice g_PhysicalDevice;
extern VkAllocationCallbacks* g_Allocator;

// WICHTIG: musst du aus deinem Hook verfügbar machen!
extern VkQueue g_Queue;
extern uint32_t g_QueueFamily;
extern VkCommandPool g_CommandPool; // <- solltest du global setzen!

// ======================================================
// TEXTURE STRUCT
// ======================================================


// ======================================================
// MEMORY HELPER
// ======================================================
uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

// ======================================================
// TEXTURE LOAD
// ======================================================
bool LoadTextureFromMemory(const unsigned char* data, int data_size, MyTextureData* out_tex) {
    int channels;

    unsigned char* pixels = stbi_load_from_memory(
        data, data_size, &out_tex->Width, &out_tex->Height, &channels, 4);

    if (!pixels)
        return false;

    VkDeviceSize image_size = out_tex->Width * out_tex->Height * 4;

    // =====================================
    // IMAGE
    // =====================================
    {
        VkImageCreateInfo info{ };
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent = {(uint32_t)out_tex->Width, (uint32_t)out_tex->Height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        vkCreateImage(g_Device, &info, g_Allocator, &out_tex->Image);

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(g_Device, out_tex->Image, &req);

        VkMemoryAllocateInfo alloc{ };
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(g_Device, &alloc, g_Allocator, &out_tex->ImageMemory);
        vkBindImageMemory(g_Device, out_tex->Image, out_tex->ImageMemory, 0);
    }

    // =====================================
    // IMAGE VIEW
    // =====================================
    {
        VkImageViewCreateInfo view{ };
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = out_tex->Image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = VK_FORMAT_R8G8B8A8_UNORM;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;

        vkCreateImageView(g_Device, &view, g_Allocator, &out_tex->ImageView);
    }

    // =====================================
    // SAMPLER (WICHTIG!)
    // =====================================
    {
        VkSamplerCreateInfo sampler{ };
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.maxLod = 1.0f;

        vkCreateSampler(g_Device, &sampler, g_Allocator, &out_tex->Sampler);
    }

    // =====================================
    // STAGING BUFFER
    // =====================================
    {
        VkBufferCreateInfo buffer{ };
        buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer.size = image_size;
        buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        vkCreateBuffer(g_Device, &buffer, g_Allocator, &out_tex->UploadBuffer);

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(g_Device, out_tex->UploadBuffer, &req);

        VkMemoryAllocateInfo alloc{ };
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = FindMemoryType(
            req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(g_Device, &alloc, g_Allocator, &out_tex->UploadBufferMemory);
        vkBindBufferMemory(g_Device, out_tex->UploadBuffer, out_tex->UploadBufferMemory, 0);

        void* map;
        vkMapMemory(g_Device, out_tex->UploadBufferMemory, 0, image_size, 0, &map);
        memcpy(map, pixels, image_size);
        vkUnmapMemory(g_Device, out_tex->UploadBufferMemory);
    }

    stbi_image_free(pixels);

    // =====================================
    // COMMAND BUFFER
    // =====================================
    VkCommandBuffer cmd;

    {
        VkCommandBufferAllocateInfo alloc{ };
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = g_CommandPool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        vkAllocateCommandBuffers(g_Device, &alloc, &cmd);

        VkCommandBufferBeginInfo begin{ };
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &begin);
    }

    // Layout Transition + Copy
    {
        VkImageMemoryBarrier barrier{ };
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = out_tex->Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{ };
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {(uint32_t)out_tex->Width, (uint32_t)out_tex->Height, 1};

        vkCmdCopyBufferToImage(cmd, out_tex->UploadBuffer, out_tex->Image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);

    {
        VkSubmitInfo submit{ };
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        vkQueueSubmit(g_Queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_Queue);
    }

    // =====================================
    // IMGUI DESCRIPTOR
    // =====================================
    out_tex->DS = ImGui_ImplVulkan_AddTexture(
        out_tex->Sampler,
        out_tex->ImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return true;
}

// ======================================================
// CLEANUP
// ======================================================
void DestroyTexture(MyTextureData* tex) {
    vkDestroySampler(g_Device, tex->Sampler, nullptr);
    vkDestroyImageView(g_Device, tex->ImageView, nullptr);
    vkDestroyImage(g_Device, tex->Image, nullptr);
    vkFreeMemory(g_Device, tex->ImageMemory, nullptr);

    vkDestroyBuffer(g_Device, tex->UploadBuffer, nullptr);
    vkFreeMemory(g_Device, tex->UploadBufferMemory, nullptr);

    ImGui_ImplVulkan_RemoveTexture(tex->DS);
}
