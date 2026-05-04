#pragma once

struct MyTextureData {
    VkDescriptorSet DS;

    int Width;
    int Height;

    VkImage Image;
    VkDeviceMemory ImageMemory;
    VkImageView ImageView;
    VkSampler Sampler;

    VkBuffer UploadBuffer;
    VkDeviceMemory UploadBufferMemory;

    MyTextureData( ) { memset(this, 0, sizeof(*this)); }
};

bool LoadTextureFromMemory(const unsigned char* data, int data_size, MyTextureData* out_tex);
void DestroyTexture(MyTextureData* tex);
