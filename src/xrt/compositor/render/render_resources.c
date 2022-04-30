// Copyright 2019-2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Shared resources for rendering.
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup comp_render
 */

#include "xrt/xrt_device.h"
#include "math/m_vec2.h"
#include "render/render_interfaces.h"

#include <stdio.h>


#define C(c)                                                                                                           \
	do {                                                                                                           \
		VkResult ret = c;                                                                                      \
		if (ret != VK_SUCCESS) {                                                                               \
			return false;                                                                                  \
		}                                                                                                      \
	} while (false)

#define D(TYPE, thing)                                                                                                 \
	if (thing != VK_NULL_HANDLE) {                                                                                 \
		vk->vkDestroy##TYPE(vk->device, thing, NULL);                                                          \
		thing = VK_NULL_HANDLE;                                                                                \
	}

#define DF(TYPE, thing)                                                                                                \
	if (thing != VK_NULL_HANDLE) {                                                                                 \
		vk->vkFree##TYPE(vk->device, thing, NULL);                                                             \
		thing = VK_NULL_HANDLE;                                                                                \
	}


/*
 *
 * Mesh
 *
 */

static VkResult
create_mesh_descriptor_set_layout(struct vk_bundle *vk,
                                  uint32_t src_binding,
                                  uint32_t ubo_binding,
                                  VkDescriptorSetLayout *out_descriptor_set_layout)
{
	VkResult ret;

	VkDescriptorSetLayoutBinding set_layout_bindings[2] = {
	    {
	        .binding = src_binding,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	    },
	    {
	        .binding = ubo_binding,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	    },
	};

	VkDescriptorSetLayoutCreateInfo set_layout_info = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	    .bindingCount = ARRAY_SIZE(set_layout_bindings),
	    .pBindings = set_layout_bindings,
	};

	VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
	ret = vk->vkCreateDescriptorSetLayout(vk->device,              //
	                                      &set_layout_info,        //
	                                      NULL,                    //
	                                      &descriptor_set_layout); //
	if (ret != VK_SUCCESS) {
		VK_ERROR(vk, "vkCreateDescriptorSetLayout failed: %s", vk_result_string(ret));
		return ret;
	}

	*out_descriptor_set_layout = descriptor_set_layout;

	return VK_SUCCESS;
}


static bool
init_mesh_vertex_buffers(struct vk_bundle *vk,
                         struct render_buffer *vbo,
                         struct render_buffer *ibo,
                         uint32_t vertex_count,
                         uint32_t stride,
                         void *vertices,
                         uint32_t index_counts,
                         void *indices)
{
	// Using the same flags for all vbos.
	VkBufferUsageFlags vbo_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VkBufferUsageFlags ibo_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VkMemoryPropertyFlags memory_property_flags =
	    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	// Distortion vbo and ibo sizes.
	VkDeviceSize vbo_size = stride * vertex_count;
	VkDeviceSize ibo_size = sizeof(int) * index_counts;


	// Don't create vbo if size is zero.
	if (vbo_size == 0) {
		return true;
	}

	C(render_buffer_init(vk,                    // vk_bundle
	                     vbo,                   // buffer
	                     vbo_usage_flags,       // usage_flags
	                     memory_property_flags, // memory_property_flags
	                     vbo_size));            // size

	C(render_buffer_write(vk,         // vk_bundle
	                      vbo,        // buffer
	                      vertices,   // data
	                      vbo_size)); // size


	// Don't create index buffer if size is zero.
	if (ibo_size == 0) {
		return true;
	}

	C(render_buffer_init(vk,                    // vk_bundle
	                     ibo,                   // buffer
	                     ibo_usage_flags,       // usage_flags
	                     memory_property_flags, // memory_property_flags
	                     ibo_size));            // size

	C(render_buffer_write(vk,         // vk_bundle
	                      ibo,        // buffer
	                      indices,    // data
	                      ibo_size)); // size

	return true;
}

static bool
init_mesh_ubo_buffers(struct vk_bundle *vk, struct render_buffer *l_ubo, struct render_buffer *r_ubo)
{
	// Using the same flags for all ubos.
	VkBufferUsageFlags ubo_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	VkMemoryPropertyFlags memory_property_flags =
	    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	// Distortion ubo size.
	VkDeviceSize ubo_size = sizeof(struct render_gfx_mesh_ubo_data);

	C(render_buffer_init(vk,                    //
	                     l_ubo,                 //
	                     ubo_usage_flags,       //
	                     memory_property_flags, //
	                     ubo_size));            // size
	C(render_buffer_map(vk, l_ubo));

	C(render_buffer_init(vk,                    //
	                     r_ubo,                 //
	                     ubo_usage_flags,       //
	                     memory_property_flags, //
	                     ubo_size));            // size
	C(render_buffer_map(vk, r_ubo));


	return true;
}


/*
 *
 * Compute
 *
 */

static VkResult
create_compute_descriptor_set_layout(struct vk_bundle *vk,
                                     uint32_t src_binding,
                                     uint32_t distortion_binding,
                                     uint32_t target_binding,
                                     uint32_t ubo_binding,
                                     VkDescriptorSetLayout *out_descriptor_set_layout)
{
	VkResult ret;

	VkDescriptorSetLayoutBinding set_layout_bindings[4] = {
	    {
	        .binding = src_binding,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .descriptorCount = 2,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    },
	    {
	        .binding = distortion_binding,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .descriptorCount = 6,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    },
	    {
	        .binding = target_binding,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    },
	    {
	        .binding = ubo_binding,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    },
	};

	VkDescriptorSetLayoutCreateInfo set_layout_info = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	    .bindingCount = ARRAY_SIZE(set_layout_bindings),
	    .pBindings = set_layout_bindings,
	};

	VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
	ret = vk->vkCreateDescriptorSetLayout( //
	    vk->device,                        //
	    &set_layout_info,                  //
	    NULL,                              //
	    &descriptor_set_layout);           //
	if (ret != VK_SUCCESS) {
		VK_ERROR(vk, "vkCreateDescriptorSetLayout failed: %s", vk_result_string(ret));
		return ret;
	}

	*out_descriptor_set_layout = descriptor_set_layout;

	return VK_SUCCESS;
}

struct compute_distortion_params
{
	uint32_t distortion_texel_count;
	VkBool32 do_timewarp;
};

static VkResult
create_compute_distortion_pipeline(struct vk_bundle *vk,
                                   VkPipelineCache pipeline_cache,
                                   VkShaderModule shader,
                                   VkPipelineLayout pipeline_layout,
                                   const struct compute_distortion_params *params,
                                   VkPipeline *out_compute_pipeline)
{
#define ENTRY(ID, FIELD)                                                                                               \
	{                                                                                                              \
	    .constantID = ID,                                                                                          \
	    .offset = offsetof(struct compute_distortion_params, FIELD),                                               \
	    sizeof(params->FIELD),                                                                                     \
	}

	VkSpecializationMapEntry entries[2] = {
	    ENTRY(0, distortion_texel_count),
	    ENTRY(1, do_timewarp),
	};
#undef ENTRY

	VkSpecializationInfo specialization_info = {
	    .mapEntryCount = ARRAY_SIZE(entries),
	    .pMapEntries = entries,
	    .dataSize = sizeof(*params),
	    .pData = params,
	};

	return vk_create_compute_pipeline( //
	    vk,                            // vk_bundle
	    pipeline_cache,                // pipeline_cache
	    shader,                        // shader
	    pipeline_layout,               // pipeline_layout
	    &specialization_info,          // specialization_info
	    out_compute_pipeline);         // out_compute_pipeline
}

static VkResult
create_distortion_image_and_view(struct vk_bundle *vk,
                                 VkExtent2D extent,
                                 VkDeviceMemory *out_device_memory,
                                 VkImage *out_image,
                                 VkImageView *out_image_view)
{
	VkFormat format = VK_FORMAT_R32G32_SFLOAT;
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory device_memory = VK_NULL_HANDLE;
	VkImageView image_view = VK_NULL_HANDLE;

	C(vk_create_image_simple(                                         //
	    vk,                                                           // vk_bundle
	    extent,                                                       // extent
	    format,                                                       // format
	    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // usage
	    &device_memory,                                               // out_device_memory
	    &image));                                                     // out_image

	VkImageSubresourceRange subresource_range = {
	    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .baseMipLevel = 0,
	    .levelCount = VK_REMAINING_MIP_LEVELS,
	    .baseArrayLayer = 0,
	    .layerCount = VK_REMAINING_ARRAY_LAYERS,
	};

	C(vk_create_view(      //
	    vk,                // vk_bundle
	    image,             // image
	    format,            // format
	    subresource_range, // subresource_range
	    &image_view));     // out_image_view

	*out_device_memory = device_memory;
	*out_image = image;
	*out_image_view = image_view;

	return VK_SUCCESS;
}

static VkResult
queue_upload_for_first_level_and_layer(
    struct vk_bundle *vk, VkCommandBuffer cmd, VkBuffer src, VkImage dst, VkExtent2D extent)
{
	VkImageSubresourceRange subresource_range = {
	    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .baseMipLevel = 0,
	    .levelCount = VK_REMAINING_MIP_LEVELS,
	    .baseArrayLayer = 0,
	    .layerCount = VK_REMAINING_ARRAY_LAYERS,
	};

	// Take the lock here.
	os_mutex_lock(&vk->cmd_pool_mutex);

	vk_cmd_image_barrier_gpu_locked(          //
	    vk,                                   //
	    cmd,                                  //
	    dst,                                  //
	    0,                                    //
	    VK_ACCESS_TRANSFER_WRITE_BIT,         //
	    VK_IMAGE_LAYOUT_UNDEFINED,            //
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, //
	    subresource_range);                   //

	VkImageSubresourceLayers subresource_layers = {
	    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .mipLevel = 0,
	    .baseArrayLayer = 0,
	    .layerCount = 1,
	};

	VkBufferImageCopy region = {
	    .bufferOffset = 0,
	    .bufferRowLength = 0,
	    .bufferImageHeight = 0,
	    .imageSubresource = subresource_layers,
	    .imageOffset = {0, 0, 0},
	    .imageExtent = {extent.width, extent.height, 1},
	};

	vk->vkCmdCopyBufferToImage(               //
	    cmd,                                  // commandBuffer
	    src,                                  // srcBuffer
	    dst,                                  // dstImage
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // dstImageLayout
	    1,                                    // regionCount
	    &region);                             // pRegions

	vk_cmd_image_barrier_gpu_locked(              //
	    vk,                                       //
	    cmd,                                      //
	    dst,                                      //
	    0,                                        //
	    VK_ACCESS_SHADER_READ_BIT,                //
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,     //
	    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, //
	    subresource_range);                       //

	// Once we are done writing commands.
	os_mutex_unlock(&vk->cmd_pool_mutex);

	return VK_SUCCESS;
}

static VkResult
create_and_queue_upload(struct vk_bundle *vk,
                        VkCommandBuffer cmd,
                        VkBuffer src_buffer,
                        VkDeviceMemory *out_image_device_memory,
                        VkImage *out_image,
                        VkImageView *out_image_view)
{
	VkExtent2D extent = {COMP_DISTORTION_IMAGE_DIMENSIONS, COMP_DISTORTION_IMAGE_DIMENSIONS};

	VkDeviceMemory device_memory = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	VkImageView image_view = VK_NULL_HANDLE;

	C(create_distortion_image_and_view( //
	    vk,                             // vk_bundle
	    extent,                         // extent
	    &device_memory,                 // out_device_memory
	    &image,                         // out_image
	    &image_view));                  // out_image_view

	C(queue_upload_for_first_level_and_layer( //
	    vk,                                   // vk_bundle
	    cmd,                                  // cmd
	    src_buffer,                           // src
	    image,                                // dst
	    extent));                             // extent

	*out_image_device_memory = device_memory;
	*out_image = image;
	*out_image_view = image_view;

	return VK_SUCCESS;
}

/*!
 * Helper struct to make code easier to read.
 */
struct texture
{
	struct xrt_vec2 pixels[COMP_DISTORTION_IMAGE_DIMENSIONS][COMP_DISTORTION_IMAGE_DIMENSIONS];
};

struct tan_angles_transforms
{
	struct xrt_vec2 offset;
	struct xrt_vec2 scale;
};

static void
calc_uv_to_tanangle(struct xrt_device *xdev, uint32_t view, struct xrt_normalized_rect *out_rect)
{
	const struct xrt_fov fov = xdev->hmd->distortion.fov[view];

	const double tan_left = tan(fov.angle_left);
	const double tan_right = tan(fov.angle_right);

	const double tan_down = tan(fov.angle_down);
	const double tan_up = tan(fov.angle_up);

	const double tan_width = tan_right - tan_left;
	const double tan_height = tan_up - tan_down;

	/*
	 * I do not know why we have to calculate the offsets like this, but
	 * this one is the one that seems to work with what is currently in the
	 * calc timewarp matrix function and the distortion shader. It works
	 * with Index (unbalanced left and right angles) and WMR (unbalanced up
	 * and down angles) so here it is. In so far it matches what the gfx
	 * and non-timewarp compute pipeline produces.
	 */
	const double tan_offset_x = ((tan_right + tan_left) - tan_width) / 2;
	const double tan_offset_y = (-(tan_up + tan_down) - tan_height) / 2;

	struct xrt_normalized_rect transform = {
	    .x = (float)tan_offset_x,
	    .y = (float)tan_offset_y,
	    .w = (float)tan_width,
	    .h = (float)tan_height,
	};

	*out_rect = transform;
}

static XRT_MAYBE_UNUSED VkResult
create_and_file_in_distortion_buffer_for_view(struct vk_bundle *vk,
                                              struct xrt_device *xdev,
                                              struct render_buffer *r_buffer,
                                              struct render_buffer *g_buffer,
                                              struct render_buffer *b_buffer,
                                              uint32_t view)
{
	VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;


	VkDeviceSize size = sizeof(struct texture);

	C(render_buffer_init(vk, r_buffer, usage_flags, properties, size));
	C(render_buffer_init(vk, g_buffer, usage_flags, properties, size));
	C(render_buffer_init(vk, b_buffer, usage_flags, properties, size));

	C(render_buffer_map(vk, r_buffer));
	C(render_buffer_map(vk, g_buffer));
	C(render_buffer_map(vk, b_buffer));

	struct texture *r = r_buffer->mapped;
	struct texture *g = g_buffer->mapped;
	struct texture *b = b_buffer->mapped;

	const double dim_minus_one_f64 = COMP_DISTORTION_IMAGE_DIMENSIONS - 1;

	for (int row = 0; row < COMP_DISTORTION_IMAGE_DIMENSIONS; row++) {
		// This goes from 0 to 1.0 inclusive.
		float v = (float)(row / dim_minus_one_f64);

		for (int col = 0; col < COMP_DISTORTION_IMAGE_DIMENSIONS; col++) {
			// This goes from 0 to 1.0 inclusive.
			float u = (float)(col / dim_minus_one_f64);

			struct xrt_uv_triplet result;
			xrt_device_compute_distortion(xdev, view, u, v, &result);


			r->pixels[row][col] = result.r;
			g->pixels[row][col] = result.g;
			b->pixels[row][col] = result.b;
		}
	}

	render_buffer_unmap(vk, r_buffer);
	render_buffer_unmap(vk, g_buffer);
	render_buffer_unmap(vk, b_buffer);

	return VK_SUCCESS;
}

/*
 *
 * 'Exported' renderer functions.
 *
 */

bool
render_resources_init(struct render_resources *r,
                      struct render_shaders *shaders,
                      struct vk_bundle *vk,
                      struct xrt_device *xdev)
{
	/*
	 * Main pointers.
	 */

	r->vk = vk;
	r->shaders = shaders;


	/*
	 * Constants
	 */

	r->mesh.src_binding = 0;
	r->mesh.ubo_binding = 1;
	struct xrt_hmd_parts *parts = xdev->hmd;
	r->mesh.vertex_count = parts->distortion.mesh.vertex_count;
	r->mesh.stride = parts->distortion.mesh.stride;
	r->mesh.index_counts[0] = parts->distortion.mesh.index_counts[0];
	r->mesh.index_counts[1] = parts->distortion.mesh.index_counts[1];
	r->mesh.index_count_total = parts->distortion.mesh.index_count_total;
	r->mesh.index_offsets[0] = parts->distortion.mesh.index_offsets[0];
	r->mesh.index_offsets[1] = parts->distortion.mesh.index_offsets[1];

	r->compute.src_binding = 0;
	r->compute.distortion_binding = 1;
	r->compute.target_binding = 2;
	r->compute.ubo_binding = 3;


	/*
	 * Dummy
	 */

	{
		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		VkExtent2D extent = {1, 1};

		VkImageSubresourceRange subresource_range = {
		    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		    .baseMipLevel = 0,
		    .levelCount = 1,
		    .baseArrayLayer = 0,
		    .layerCount = 1,
		};

		C(vk_create_image_simple(    //
		    vk,                      // vk_bundle
		    extent,                  // extent
		    format,                  // format
		    usage,                   // usage
		    &r->dummy.color.memory,  // out_mem
		    &r->dummy.color.image)); // out_image

		C(vk_create_view(                 //
		    vk,                           // vk_bundle
		    r->dummy.color.image,         // image
		    format,                       // format
		    subresource_range,            // subresource_range
		    &r->dummy.color.image_view)); // out_view
	}


	/*
	 * Shared
	 */

	C(vk_create_pipeline_cache(vk, &r->pipeline_cache));


	/*
	 * Mesh static.
	 */

	struct vk_descriptor_pool_info mesh_pool_info = {
	    .uniform_per_descriptor_count = 1,
	    .sampler_per_descriptor_count = 1,
	    .storage_image_per_descriptor_count = 0,
	    .storage_buffer_per_descriptor_count = 0,
	    .descriptor_count = 16 * 2,
	    .freeable = false,
	};

	C(vk_create_descriptor_pool(    //
	    vk,                         // vk_bundle
	    &mesh_pool_info,            // info
	    &r->mesh.descriptor_pool)); // out_descriptor_pool

	C(create_mesh_descriptor_set_layout(  //
	    vk,                               // vk_bundle
	    r->mesh.src_binding,              // src_binding
	    r->mesh.ubo_binding,              // ubo_binding
	    &r->mesh.descriptor_set_layout)); // out_mesh_descriptor_set_layout

	C(vk_create_pipeline_layout(       //
	    vk,                            // vk_bundle
	    r->mesh.descriptor_set_layout, // descriptor_set_layout
	    &r->mesh.pipeline_layout));    // out_pipeline_layout

	if (!init_mesh_vertex_buffers(vk,                                //
	                              &r->mesh.vbo,                      //
	                              &r->mesh.ibo,                      //
	                              r->mesh.vertex_count,              //
	                              r->mesh.stride,                    //
	                              parts->distortion.mesh.vertices,   //
	                              r->mesh.index_count_total,         //
	                              parts->distortion.mesh.indices)) { //
		return false;
	}

	if (!init_mesh_ubo_buffers(vk,               //
	                           &r->mesh.ubos[0], //
	                           &r->mesh.ubos[1])) {
		return false;
	}


	/*
	 * Compute static.
	 */

	C(vk_create_sampler(                       //
	    vk,                                    // vk_bundle
	    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // clamp_mode
	    &r->compute.default_sampler));         // out_sampler

	struct vk_descriptor_pool_info compute_pool_info = {
	    .uniform_per_descriptor_count = 1,
	    .sampler_per_descriptor_count = 8,
	    .storage_image_per_descriptor_count = 1,
	    .storage_buffer_per_descriptor_count = 0,
	    .descriptor_count = 1,
	    .freeable = false,
	};

	C(vk_create_descriptor_pool(       //
	    vk,                            // vk_bundle
	    &compute_pool_info,            // info
	    &r->compute.descriptor_pool)); // out_descriptor_pool

	C(create_compute_descriptor_set_layout(  //
	    vk,                                  // vk_bundle
	    r->compute.src_binding,              // src_binding,
	    r->compute.distortion_binding,       // distortion_binding,
	    r->compute.target_binding,           // target_binding,
	    r->compute.ubo_binding,              // ubo_binding,
	    &r->compute.descriptor_set_layout)); // out_descriptor_set_layout

	C(vk_create_pipeline_layout(          //
	    vk,                               // vk_bundle
	    r->compute.descriptor_set_layout, // descriptor_set_layout
	    &r->compute.pipeline_layout));    // out_pipeline_layout

	C(vk_create_compute_pipeline(     //
	    vk,                           // vk_bundle
	    r->pipeline_cache,            // pipeline_cache
	    r->shaders->clear_comp,       // shader
	    r->compute.pipeline_layout,   // pipeline_layout
	    NULL,                         // specialization_info
	    &r->compute.clear_pipeline)); // out_compute_pipeline

	struct compute_distortion_params distortion_params = {
	    .distortion_texel_count = COMP_DISTORTION_IMAGE_DIMENSIONS,
	    .do_timewarp = false,
	};

	C(create_compute_distortion_pipeline(  //
	    vk,                                // vk_bundle
	    r->pipeline_cache,                 // pipeline_cache
	    r->shaders->distortion_comp,       // shader
	    r->compute.pipeline_layout,        // pipeline_layout
	    &distortion_params,                // params
	    &r->compute.distortion_pipeline)); // out_compute_pipeline

	struct compute_distortion_params distortion_timewarp_params = {
	    .distortion_texel_count = COMP_DISTORTION_IMAGE_DIMENSIONS,
	    .do_timewarp = true,
	};

	C(create_compute_distortion_pipeline(           //
	    vk,                                         // vk_bundle
	    r->pipeline_cache,                          // pipeline_cache
	    r->shaders->distortion_comp,                // shader
	    r->compute.pipeline_layout,                 // pipeline_layout
	    &distortion_timewarp_params,                // params
	    &r->compute.distortion_timewarp_pipeline)); // out_compute_pipeline


	VkBufferUsageFlags ubo_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	VkMemoryPropertyFlags memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
	                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
	                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	size_t ubo_size = sizeof(struct render_compute_distortion_ubo_data);

	C(render_buffer_init(      //
	    vk,                    // vk_bundle
	    &r->compute.ubo,       // buffer
	    ubo_usage_flags,       // usage_flags
	    memory_property_flags, // memory_property_flags
	    ubo_size));            // size
	C(render_buffer_map(       //
	    vk,                    // vk_bundle
	    &r->compute.ubo));     // buffer


	struct render_buffer buffers[COMP_DISTORTION_NUM_IMAGES];

	calc_uv_to_tanangle(xdev, 0, &r->distortion.uv_to_tanangle[0]);
	calc_uv_to_tanangle(xdev, 1, &r->distortion.uv_to_tanangle[1]);

	create_and_file_in_distortion_buffer_for_view(vk, xdev, &buffers[0], &buffers[2], &buffers[4], 0);
	create_and_file_in_distortion_buffer_for_view(vk, xdev, &buffers[1], &buffers[3], &buffers[5], 1);

	VkCommandBuffer upload_buffer = VK_NULL_HANDLE;
	C(vk_init_cmd_buffer(vk, &upload_buffer));

	for (uint32_t i = 0; i < COMP_DISTORTION_NUM_IMAGES; i++) {
		C(create_and_queue_upload(             //
		    vk,                                // vk_bundle
		    upload_buffer,                     // cmd
		    buffers[i].buffer,                 // src_buffer
		    &r->distortion.device_memories[i], // out_image_device_memory
		    &r->distortion.images[i],          // out_image
		    &r->distortion.image_views[i]));   // out_image_view
	}

	C(vk_submit_cmd_buffer(vk, upload_buffer));

	os_mutex_lock(&vk->queue_mutex);
	vk->vkDeviceWaitIdle(vk->device);
	os_mutex_unlock(&vk->queue_mutex);

	for (uint32_t i = 0; i < ARRAY_SIZE(buffers); i++) {
		render_buffer_close(vk, &buffers[i]);
	}


	/*
	 * Done
	 */

	U_LOG_I("New renderer initialized!");

	return true;
}

void
render_resources_close(struct render_resources *r)
{
	// We were never initialised or already closed, always safe to call this function.
	if (r->vk == NULL) {
		return;
	}

	struct vk_bundle *vk = r->vk;

	D(ImageView, r->dummy.color.image_view);
	D(Image, r->dummy.color.image);
	DF(Memory, r->dummy.color.memory);
	D(DescriptorSetLayout, r->mesh.descriptor_set_layout);
	D(PipelineLayout, r->mesh.pipeline_layout);
	D(PipelineCache, r->pipeline_cache);
	D(DescriptorPool, r->mesh.descriptor_pool);
	render_buffer_close(vk, &r->mesh.vbo);
	render_buffer_close(vk, &r->mesh.ibo);
	render_buffer_close(vk, &r->mesh.ubos[0]);
	render_buffer_close(vk, &r->mesh.ubos[1]);

	D(DescriptorPool, r->compute.descriptor_pool);
	D(DescriptorSetLayout, r->compute.descriptor_set_layout);
	D(Pipeline, r->compute.clear_pipeline);
	D(Pipeline, r->compute.distortion_pipeline);
	D(Pipeline, r->compute.distortion_timewarp_pipeline);
	D(PipelineLayout, r->compute.pipeline_layout);
	D(Sampler, r->compute.default_sampler);
	for (uint32_t i = 0; i < ARRAY_SIZE(r->distortion.image_views); i++) {
		D(ImageView, r->distortion.image_views[i]);
	}
	for (uint32_t i = 0; i < ARRAY_SIZE(r->distortion.images); i++) {
		D(Image, r->distortion.images[i]);
	}
	for (uint32_t i = 0; i < ARRAY_SIZE(r->distortion.images); i++) {
		DF(Memory, r->distortion.device_memories[i]);
	}
	render_buffer_close(vk, &r->compute.ubo);

	// Finally forget about the vk bundle. We do not own it!
	r->vk = NULL;
}
