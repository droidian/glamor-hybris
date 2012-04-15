#include <stdlib.h>

#include "glamor_priv.h"
/**
 * Sets the offsets to add to coordinates to make them address the same bits in
 * the backing drawable. These coordinates are nonzero only for redirected
 * windows.
 */
void
glamor_get_drawable_deltas(DrawablePtr drawable, PixmapPtr pixmap,
			   int *x, int *y)
{
#ifdef COMPOSITE
	if (drawable->type == DRAWABLE_WINDOW) {
		*x = -pixmap->screen_x;
		*y = -pixmap->screen_y;
		return;
	}
#endif

	*x = 0;
	*y = 0;
}


static void
_glamor_pixmap_validate_filling(glamor_screen_private * glamor_priv,
				glamor_pixmap_private * pixmap_priv)
{
	glamor_gl_dispatch *dispatch = glamor_get_dispatch(glamor_priv);
	GLfloat vertices[8];
	dispatch->glVertexAttribPointer(GLAMOR_VERTEX_POS, 2, GL_FLOAT,
					GL_FALSE, 2 * sizeof(float),
					vertices);
	dispatch->glEnableVertexAttribArray(GLAMOR_VERTEX_POS);
	dispatch->glUseProgram(glamor_priv->solid_prog);
	dispatch->glUniform4fv(glamor_priv->solid_color_uniform_location,
			       1, pixmap_priv->pending_op.fill.color4fv);
	vertices[0] = -1;
	vertices[1] = -1;
	vertices[2] = 1;
	vertices[3] = -1;
	vertices[4] = 1;
	vertices[5] = 1;
	vertices[6] = -1;
	vertices[7] = 1;
	dispatch->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	dispatch->glDisableVertexAttribArray(GLAMOR_VERTEX_POS);
	dispatch->glUseProgram(0);
	pixmap_priv->pending_op.type = GLAMOR_PENDING_NONE;
	glamor_put_dispatch(glamor_priv);
}


glamor_pixmap_validate_function_t pixmap_validate_funcs[] = {
	NULL,
	_glamor_pixmap_validate_filling
};

void
glamor_pixmap_init(ScreenPtr screen)
{
	glamor_screen_private *glamor_priv;

	glamor_priv = glamor_get_screen_private(screen);
	glamor_priv->pixmap_validate_funcs = pixmap_validate_funcs;
}

void
glamor_pixmap_fini(ScreenPtr screen)
{
}

void
glamor_validate_pixmap(PixmapPtr pixmap)
{
	glamor_pixmap_validate_function_t validate_op;
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(pixmap->drawable.pScreen);
	glamor_pixmap_private *pixmap_priv =
	    glamor_get_pixmap_private(pixmap);

	validate_op =
	    glamor_priv->pixmap_validate_funcs[pixmap_priv->
					       pending_op.type];
	if (validate_op) {
		(*validate_op) (glamor_priv, pixmap_priv);
	}
}

void
glamor_set_destination_pixmap_fbo(glamor_pixmap_fbo * fbo)
{
	glamor_gl_dispatch *dispatch = glamor_get_dispatch(fbo->glamor_priv);
	dispatch->glBindFramebuffer(GL_FRAMEBUFFER, fbo->fb);
#ifndef GLAMOR_GLES2
	dispatch->glMatrixMode(GL_PROJECTION);
	dispatch->glLoadIdentity();
	dispatch->glMatrixMode(GL_MODELVIEW);
	dispatch->glLoadIdentity();
#endif
	dispatch->glViewport(0, 0,
			     fbo->width,
			     fbo->height);

	glamor_put_dispatch(fbo->glamor_priv);
}

void
glamor_set_destination_pixmap_priv_nc(glamor_pixmap_private * pixmap_priv)
{
	glamor_set_destination_pixmap_fbo(pixmap_priv->fbo);
}

int
glamor_set_destination_pixmap_priv(glamor_pixmap_private * pixmap_priv)
{
	if (!GLAMOR_PIXMAP_PRIV_HAS_FBO(pixmap_priv))
		return -1;

	glamor_set_destination_pixmap_priv_nc(pixmap_priv);
	return 0;
}

int
glamor_set_destination_pixmap(PixmapPtr pixmap)
{
	int err;
	glamor_pixmap_private *pixmap_priv =
	    glamor_get_pixmap_private(pixmap);

	err = glamor_set_destination_pixmap_priv(pixmap_priv);
	return err;
}

Bool
glamor_set_planemask(PixmapPtr pixmap, unsigned long planemask)
{
	if (glamor_pm_is_solid(&pixmap->drawable, planemask)) {
		return GL_TRUE;
	}

	glamor_fallback("unsupported planemask %lx\n", planemask);
	return GL_FALSE;
}

Bool
glamor_set_alu(struct glamor_gl_dispatch *dispatch, unsigned char alu)
{
#ifndef GLAMOR_GLES2
	if (alu == GXcopy) {
		dispatch->glDisable(GL_COLOR_LOGIC_OP);
		return TRUE;
	}
	dispatch->glEnable(GL_COLOR_LOGIC_OP);
	switch (alu) {
	case GXclear:
		dispatch->glLogicOp(GL_CLEAR);
		break;
	case GXand:
		dispatch->glLogicOp(GL_AND);
		break;
	case GXandReverse:
		dispatch->glLogicOp(GL_AND_REVERSE);
		break;
	case GXandInverted:
		dispatch->glLogicOp(GL_AND_INVERTED);
		break;
	case GXnoop:
		dispatch->glLogicOp(GL_NOOP);
		break;
	case GXxor:
		dispatch->glLogicOp(GL_XOR);
		break;
	case GXor:
		dispatch->glLogicOp(GL_OR);
		break;
	case GXnor:
		dispatch->glLogicOp(GL_NOR);
		break;
	case GXequiv:
		dispatch->glLogicOp(GL_EQUIV);
		break;
	case GXinvert:
		dispatch->glLogicOp(GL_INVERT);
		break;
	case GXorReverse:
		dispatch->glLogicOp(GL_OR_REVERSE);
		break;
	case GXcopyInverted:
		dispatch->glLogicOp(GL_COPY_INVERTED);
		break;
	case GXorInverted:
		dispatch->glLogicOp(GL_OR_INVERTED);
		break;
	case GXnand:
		dispatch->glLogicOp(GL_NAND);
		break;
	case GXset:
		dispatch->glLogicOp(GL_SET);
		break;
	default:
		glamor_fallback("unsupported alu %x\n", alu);
		return FALSE;
	}
#else
	if (alu != GXcopy)
		return FALSE;
#endif
	return TRUE;
}

void *
_glamor_color_convert_a1_a8(void *src_bits, void *dst_bits, int w, int h, int stride, int revert)
{
	void *bits;
	PictFormatShort dst_format, src_format;
	pixman_image_t *dst_image;
	pixman_image_t *src_image;
	int src_stride;

	if (revert == REVERT_UPLOADING_A1) {
		src_format = PICT_a1;
		dst_format = PICT_a8;
		src_stride = PixmapBytePad(w, 1);
	} else {
		dst_format = PICT_a1;
		src_format = PICT_a8;
		src_stride = (((w * 8 + 7) / 8) + 3) & ~3;
	}

	dst_image = pixman_image_create_bits(dst_format,
					     w, h,
					     dst_bits,
					     stride);
	if (dst_image == NULL) {
		free(bits);
		return NULL;
	}

	src_image = pixman_image_create_bits(src_format,
					     w, h,
					     src_bits,
					     src_stride);

	if (src_image == NULL) {
		pixman_image_unref(dst_image);
		free(bits);
		return NULL;
	}

	pixman_image_composite(PictOpSrc, src_image, NULL, dst_image,
			       0, 0, 0, 0, 0, 0,
			       w,h);

	pixman_image_unref(src_image);
	pixman_image_unref(dst_image);
	return dst_bits;
}

#define ADJUST_BITS(d, src_bits, dst_bits)	(((dst_bits) == (src_bits)) ? (d) : 				\
							(((dst_bits) > (src_bits)) ? 				\
							  (((d) << ((dst_bits) - (src_bits))) 			\
								   + (( 1 << ((dst_bits) - (src_bits))) >> 1))	\
								:  ((d) >> ((src_bits) - (dst_bits)))))

#define GLAMOR_DO_CONVERT(src, dst, no_alpha, swap,		\
			  a_shift_src, a_bits_src,		\
			  b_shift_src, b_bits_src,		\
			  g_shift_src, g_bits_src,		\
			  r_shift_src, r_bits_src,		\
			  a_shift, a_bits,			\
			  b_shift, b_bits,			\
			  g_shift, g_bits,			\
			  r_shift, r_bits)			\
	{								\
		typeof(src) a,b,g,r;					\
		typeof(src) a_mask_src, b_mask_src, g_mask_src, r_mask_src;\
		a_mask_src = (((1 << (a_bits_src)) - 1) << a_shift_src);\
		b_mask_src = (((1 << (b_bits_src)) - 1) << b_shift_src);\
		g_mask_src = (((1 << (g_bits_src)) - 1) << g_shift_src);\
		r_mask_src = (((1 << (r_bits_src)) - 1) << r_shift_src);\
		if (no_alpha)						\
			a = (a_mask_src) >> (a_shift_src);			\
		else							\
			a = ((src) & (a_mask_src)) >> (a_shift_src);	\
		b = ((src) & (b_mask_src)) >> (b_shift_src);		\
		g = ((src) & (g_mask_src)) >> (g_shift_src);		\
		r = ((src) & (r_mask_src)) >> (r_shift_src);		\
		a = ADJUST_BITS(a, a_bits_src, a_bits);			\
		b = ADJUST_BITS(b, b_bits_src, b_bits);			\
		g = ADJUST_BITS(g, g_bits_src, g_bits);			\
		r = ADJUST_BITS(r, r_bits_src, r_bits);			\
		if (swap == 0)						\
			(*dst) = ((a) << (a_shift)) | ((b) << (b_shift)) | ((g) << (g_shift)) | ((r) << (r_shift)); \
		else 												    \
			(*dst) = ((a) << (a_shift)) | ((r) << (b_shift)) | ((g) << (g_shift)) | ((b) << (r_shift)); \
	}

void *
_glamor_color_revert_x2b10g10r10(void *src_bits, void *dst_bits, int w, int h, int stride, int no_alpha, int revert, int swap_rb)
{
	int x,y;
	unsigned int *words, *saved_words, *source_words;
	int swap = !(swap_rb == SWAP_NONE_DOWNLOADING || swap_rb == SWAP_NONE_UPLOADING);

	source_words = src_bits;
	words = dst_bits;
	saved_words = words;

	for (y = 0; y < h; y++)
	{
		DEBUGF("Line %d :  ", y);
		for (x = 0; x < w; x++)
		{
			unsigned int pixel = source_words[x];

			if (revert == REVERT_DOWNLOADING_2_10_10_10)
				GLAMOR_DO_CONVERT(pixel, &words[x], no_alpha, swap,
						  24, 8, 16, 8, 8, 8, 0, 8,
						  30, 2, 20, 10, 10, 10, 0, 10)
			else
				GLAMOR_DO_CONVERT(pixel, &words[x], no_alpha, swap,
						  30, 2, 20, 10, 10, 10, 0, 10,
						  24, 8, 16, 8, 8, 8, 0, 8);
			DEBUGF("%x:%x ", pixel, words[x]);
		}
		DEBUGF("\n");
		words += stride / sizeof(*words);
		source_words += stride / sizeof(*words);
	}
	DEBUGF("\n");
	return saved_words;

}

void *
_glamor_color_revert_x1b5g5r5(void *src_bits, void *dst_bits, int w, int h, int stride, int no_alpha, int revert, int swap_rb)
{
	int x,y;
	unsigned short *words, *saved_words, *source_words;
	int swap = !(swap_rb == SWAP_NONE_DOWNLOADING || swap_rb == SWAP_NONE_UPLOADING);

	words = dst_bits;
	source_words = src_bits;
	saved_words = words;

	for (y = 0; y < h; y++)
	{
		DEBUGF("Line %d :  ", y);
		for (x = 0; x < w; x++)
		{
			unsigned short pixel = source_words[x];

			if (revert == REVERT_DOWNLOADING_1_5_5_5)
				GLAMOR_DO_CONVERT(pixel, &words[x], no_alpha, swap,
						  0, 1, 1, 5, 6, 5, 11, 5,
						  15, 1, 10, 5, 5, 5, 0, 5)
			else
				GLAMOR_DO_CONVERT(pixel, &words[x], no_alpha, swap,
						  15, 1, 10, 5, 5, 5, 0, 5,
						  0, 1, 1, 5, 6, 5, 11, 5);
			DEBUGF("%04x:%04x ", pixel, words[x]);
		}
		DEBUGF("\n");
		words += stride / sizeof(*words);
		source_words += stride / sizeof(*words);
	}
	DEBUGF("\n");
	return saved_words;
}

/*
 * This function is to convert an unsupported color format to/from a
 * supported GL format.
 * Here are the current scenarios:
 *
 * @no_alpha:
 * 	If it is set, then we need to wire the alpha value to 1.
 * @revert:
	REVERT_DOWNLOADING_A1		: convert an Alpha8 buffer to a A1 buffer.
	REVERT_UPLOADING_A1		: convert an A1 buffer to an Alpha8 buffer
	REVERT_DOWNLOADING_2_10_10_10 	: convert r10G10b10X2 to X2B10G10R10
	REVERT_UPLOADING_2_10_10_10 	: convert X2B10G10R10 to R10G10B10X2
	REVERT_DOWNLOADING_1_5_5_5  	: convert B5G5R5X1 to X1R5G5B5
	REVERT_UPLOADING_1_5_5_5    	: convert X1R5G5B5 to B5G5R5X1
   @swap_rb: if we have the swap_rb set, then we need to swap the R and B's position.
 *
 */

void *
glamor_color_convert_to_bits(void *src_bits, void *dst_bits, int w, int h, int stride, int no_alpha, int revert, int swap_rb)
{
	if (revert == REVERT_DOWNLOADING_A1 || revert == REVERT_UPLOADING_A1) {
		return _glamor_color_convert_a1_a8(src_bits, dst_bits, w, h, stride, revert);
	} else if (revert == REVERT_DOWNLOADING_2_10_10_10 || revert == REVERT_UPLOADING_2_10_10_10) {
		return _glamor_color_revert_x2b10g10r10(src_bits, dst_bits, w, h, stride, no_alpha, revert, swap_rb);
	} else if (revert == REVERT_DOWNLOADING_1_5_5_5 || revert == REVERT_UPLOADING_1_5_5_5) {
		return _glamor_color_revert_x1b5g5r5(src_bits, dst_bits, w, h, stride, no_alpha, revert, swap_rb);
	} else
		ErrorF("convert a non-supported mode %x.\n", revert);

	return NULL;
}

/**
 * Upload pixmap to a specified texture.
 * This texture may not be the one attached to it.
 **/
int in_restore = 0;
static void
__glamor_upload_pixmap_to_texture(PixmapPtr pixmap, int *tex,
				  GLenum format,
				  GLenum type,
				  int x, int y, int w, int h,
				  void *bits, int pbo)
{
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(pixmap->drawable.pScreen);
	glamor_gl_dispatch *dispatch;
	int non_sub = 0;
	int iformat;

	dispatch = glamor_get_dispatch(glamor_priv);
	if (*tex == 0) {
		dispatch->glGenTextures(1, tex);
		if (glamor_priv->gl_flavor == GLAMOR_GL_DESKTOP)
			gl_iformat_for_depth(pixmap->drawable.depth, &iformat);
		else
			iformat = format;
		non_sub = 1;
		assert(x == 0 && y == 0);
	}

	dispatch->glBindTexture(GL_TEXTURE_2D, *tex);
	dispatch->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				  GL_NEAREST);
	dispatch->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				  GL_NEAREST);
	dispatch->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	if (bits == NULL)
		dispatch->glBindBuffer(GL_PIXEL_UNPACK_BUFFER,
				       pbo);
	if (non_sub)
		dispatch->glTexImage2D(GL_TEXTURE_2D,
				       0, iformat, w, h, 0,
				       format, type,
				       bits);
	else
		dispatch->glTexSubImage2D(GL_TEXTURE_2D,
					  0, x, y, w, h,
					  format, type,
					  bits);

	if (bits == NULL)
		dispatch->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glamor_put_dispatch(glamor_priv);
}

static Bool
_glamor_upload_bits_to_pixmap_texture(PixmapPtr pixmap, GLenum format, GLenum type,
				     int no_alpha, int revert,
				     int swap_rb, int x, int y, int w, int h,
				     int stride, void* bits, int pbo)
{
	glamor_pixmap_private *pixmap_priv = glamor_get_pixmap_private(pixmap);
	glamor_screen_private *glamor_priv = glamor_get_screen_private(pixmap->drawable.pScreen);
	glamor_gl_dispatch *dispatch;
	static float vertices[8];
	static float texcoords[8] = { 0, 1,
		1, 1,
		1, 0,
		0, 0
	};
	static float texcoords_inv[8] = { 0, 0,
		1, 0,
		1, 1,
		0, 1
	};
	float *ptexcoords;
	float dst_xscale, dst_yscale;
	GLuint tex = 0;
	int need_flip;
	int need_free_bits = 0;

	need_flip = !glamor_priv->yInverted;

	if (bits == NULL)
		goto ready_to_upload;

	if (glamor_priv->gl_flavor == GLAMOR_GL_ES2
	    &&  revert > REVERT_NORMAL) {
		/* XXX if we are restoring the pixmap, then we may not need to allocate
		 * new buffer */
		void *converted_bits;

		if (pixmap->drawable.depth == 1)
			stride = (((w * 8 + 7) / 8) + 3) & ~3;

		converted_bits = malloc(h * stride);

		if (converted_bits == NULL)
			return FALSE;
		bits = glamor_color_convert_to_bits(bits, converted_bits, w, h,
						    stride,
						    no_alpha, revert, swap_rb);
		if (bits == NULL) {
			ErrorF("Failed to convert pixmap no_alpha %d, revert mode %d, swap mode %d\n", swap_rb);
			return FALSE;
		}
		no_alpha = 0;
		revert = REVERT_NONE;
		swap_rb = SWAP_NONE_UPLOADING;
		need_free_bits = TRUE;
	}

ready_to_upload:
	/* Try fast path firstly, upload the pixmap to the texture attached
	 * to the fbo directly. */
	if (no_alpha == 0
	    && revert == REVERT_NONE
	    && swap_rb == SWAP_NONE_UPLOADING
	    && !need_flip) {
		assert(pixmap_priv->fbo->tex);
		__glamor_upload_pixmap_to_texture(pixmap, &pixmap_priv->fbo->tex,
						  format, type,
						  x, y, w, h,
						  bits, pbo);
		return TRUE;
	}

	if (need_flip)
		ptexcoords = texcoords;
	else
		ptexcoords = texcoords_inv;

	pixmap_priv_get_scale(pixmap_priv, &dst_xscale, &dst_yscale);
	glamor_set_normalize_vcoords(dst_xscale,
				     dst_yscale,
				     x, y,
				     x + w, y + h,
				     glamor_priv->yInverted,
				     vertices);

	/* Slow path, we need to flip y or wire alpha to 1. */
	dispatch = glamor_get_dispatch(glamor_priv);
	dispatch->glVertexAttribPointer(GLAMOR_VERTEX_POS, 2, GL_FLOAT,
					GL_FALSE, 2 * sizeof(float),
					vertices);
	dispatch->glEnableVertexAttribArray(GLAMOR_VERTEX_POS);
	dispatch->glVertexAttribPointer(GLAMOR_VERTEX_SOURCE, 2, GL_FLOAT,
					GL_FALSE, 2 * sizeof(float),
					ptexcoords);
	dispatch->glEnableVertexAttribArray(GLAMOR_VERTEX_SOURCE);

	glamor_set_destination_pixmap_priv_nc(pixmap_priv);
	__glamor_upload_pixmap_to_texture(pixmap, &tex,
					  format, type,
					  0, 0, w, h,
					  bits, pbo);
	dispatch->glActiveTexture(GL_TEXTURE0);
	dispatch->glBindTexture(GL_TEXTURE_2D, tex);

	dispatch->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				  GL_NEAREST);
	dispatch->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				  GL_NEAREST);
#ifndef GLAMOR_GLES2
	dispatch->glEnable(GL_TEXTURE_2D);
#endif
	dispatch->glUseProgram(glamor_priv->finish_access_prog[no_alpha]);
	dispatch->glUniform1i(glamor_priv->
			      finish_access_revert[no_alpha],
			      revert);
	dispatch->glUniform1i(glamor_priv->finish_access_swap_rb[no_alpha],
			      swap_rb);

	dispatch->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

#ifndef GLAMOR_GLES2
	dispatch->glDisable(GL_TEXTURE_2D);
#endif
	dispatch->glUseProgram(0);
	dispatch->glDisableVertexAttribArray(GLAMOR_VERTEX_POS);
	dispatch->glDisableVertexAttribArray(GLAMOR_VERTEX_SOURCE);
	dispatch->glDeleteTextures(1, &tex);
	dispatch->glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glamor_put_dispatch(glamor_priv);

	if (need_free_bits)
		free(bits);
	return TRUE;
}

/*
 * Prepare to upload a pixmap to texture memory.
 * no_alpha equals 1 means the format needs to wire alpha to 1.
 * Two condtion need to setup a fbo for a pixmap
 * 1. !yInverted, we need to do flip if we are not yInverted.
 * 2. no_alpha != 0, we need to wire the alpha.
 * */
static int
glamor_pixmap_upload_prepare(PixmapPtr pixmap, GLenum format, int no_alpha, int revert, int swap_rb)
{
	int flag = 0;
	glamor_pixmap_private *pixmap_priv;
	glamor_screen_private *glamor_priv;
	glamor_pixmap_fbo *fbo;
	GLenum iformat;

	pixmap_priv = glamor_get_pixmap_private(pixmap);
	glamor_priv = glamor_get_screen_private(pixmap->drawable.pScreen);

	if (pixmap_priv && pixmap_priv->fbo && pixmap_priv->fbo->fb)
		return 0;

	if (!(no_alpha
	      || (revert != REVERT_NONE)
	      || (swap_rb != SWAP_NONE_UPLOADING)
	      || !glamor_priv->yInverted)) {
		/* We don't need a fbo, a simple texture uploading should work. */

		flag = GLAMOR_CREATE_FBO_NO_FBO;
	}

	if ((flag == 0 && pixmap_priv && pixmap_priv->fbo && pixmap_priv->fbo->tex)
	    || (flag != 0 && pixmap_priv && pixmap_priv->fbo && pixmap_priv->fbo->fb))
		return 0;

	if (glamor_priv->gl_flavor == GLAMOR_GL_DESKTOP)
		gl_iformat_for_depth(pixmap->drawable.depth, &iformat);
	else
		iformat = format;

	if (pixmap_priv == NULL || pixmap_priv->fbo == NULL) {

		fbo = glamor_create_fbo(glamor_priv, pixmap->drawable.width,
					pixmap->drawable.height,
					iformat,
					flag);
		if (fbo == NULL) {
			glamor_fallback
			    ("upload failed, depth %d x %d @depth %d \n",
			     pixmap->drawable.width, pixmap->drawable.height,
			     pixmap->drawable.depth);
			return -1;
		}

		glamor_pixmap_attach_fbo(pixmap, fbo);
	} else {
		/* We do have a fbo, but it may lack of fb or tex. */
		glamor_pixmap_ensure_fbo(pixmap, iformat, flag);
	}

	return 0;
}

Bool
glamor_upload_sub_pixmap_to_texture(PixmapPtr pixmap, int x, int y, int w, int h,
				    int stride, void *bits, int pbo)
{
	GLenum format, type;
	int no_alpha, revert, swap_rb;

	if (glamor_get_tex_format_type_from_pixmap(pixmap,
						   &format,
						   &type,
						   &no_alpha,
						   &revert,
						   &swap_rb, 1)) {
		glamor_fallback("Unknown pixmap depth %d.\n",
				pixmap->drawable.depth);
		return TRUE;
	}
	if (glamor_pixmap_upload_prepare(pixmap, format, no_alpha, revert, swap_rb))
		return FALSE;

	return _glamor_upload_bits_to_pixmap_texture(pixmap, format, type, no_alpha, revert, swap_rb,
						     x, y, w, h, stride, bits, pbo);
}

enum glamor_pixmap_status
glamor_upload_pixmap_to_texture(PixmapPtr pixmap)
{
	glamor_pixmap_private *pixmap_priv;
	void *data;
	int pbo;
	int ret;

	pixmap_priv = glamor_get_pixmap_private(pixmap);

	if (pixmap_priv
	    && (pixmap_priv->fbo)
	    && (pixmap_priv->fbo->pbo_valid)) {
		data = NULL;
		pbo = pixmap_priv->fbo->pbo;
	} else {
		data = pixmap->devPrivate.ptr;
		pbo = 0;
	}

	if (glamor_upload_sub_pixmap_to_texture(pixmap, 0, 0,
						pixmap->drawable.width,
						pixmap->drawable.height,
						pixmap->devKind,
						data, pbo))
		ret = GLAMOR_UPLOAD_DONE;
	else
		ret = GLAMOR_UPLOAD_FAILED;

	return ret;
}

void
glamor_restore_pixmap_to_texture(PixmapPtr pixmap)
{
	if (glamor_upload_pixmap_to_texture(pixmap) != GLAMOR_UPLOAD_DONE)
		LogMessage(X_WARNING, "Failed to restore pixmap to texture.\n",
			   pixmap->drawable.pScreen->myNum);
}

/*
 * as gles2 only support a very small set of color format and
 * type when do glReadPixel,
 * Before we use glReadPixels to get back a textured pixmap,
 * Use shader to convert it to a supported format and thus
 * get a new temporary pixmap returned.
 * */

glamor_pixmap_fbo *
glamor_es2_pixmap_read_prepare(PixmapPtr source, int x, int y, int w, int h, GLenum format,
			       GLenum type, int no_alpha, int revert, int swap_rb)

{
	glamor_pixmap_private *source_priv;
	glamor_screen_private *glamor_priv;
	ScreenPtr screen;
	glamor_pixmap_fbo *temp_fbo;
	glamor_gl_dispatch *dispatch;
	float temp_xscale, temp_yscale, source_xscale, source_yscale;
	static float vertices[8];
	static float texcoords[8];

	screen = source->drawable.pScreen;

	glamor_priv = glamor_get_screen_private(screen);
	source_priv = glamor_get_pixmap_private(source);
	temp_fbo = glamor_create_fbo(glamor_priv,
				     w, h,
				     format,
				     0);
	if (temp_fbo == NULL)
		return NULL;

	dispatch = glamor_get_dispatch(glamor_priv);
	temp_xscale = 1.0 / temp_fbo->width;
	temp_yscale = 1.0 / temp_fbo->height;

	glamor_set_normalize_vcoords(temp_xscale,
				     temp_yscale,
				     0, 0,
				     w, h,
				     glamor_priv->yInverted,
				     vertices);

	dispatch->glVertexAttribPointer(GLAMOR_VERTEX_POS, 2, GL_FLOAT,
					GL_FALSE, 2 * sizeof(float),
					vertices);
	dispatch->glEnableVertexAttribArray(GLAMOR_VERTEX_POS);

	pixmap_priv_get_scale(source_priv, &source_xscale, &source_yscale);
	glamor_set_normalize_tcoords(source_xscale,
				     source_yscale,
				     x, y,
				     x + w, y + h,
				     glamor_priv->yInverted,
				     texcoords);

	dispatch->glVertexAttribPointer(GLAMOR_VERTEX_SOURCE, 2, GL_FLOAT,
					GL_FALSE, 2 * sizeof(float),
					texcoords);
	dispatch->glEnableVertexAttribArray(GLAMOR_VERTEX_SOURCE);

	dispatch->glActiveTexture(GL_TEXTURE0);
	dispatch->glBindTexture(GL_TEXTURE_2D, source_priv->fbo->tex);
	dispatch->glTexParameteri(GL_TEXTURE_2D,
				  GL_TEXTURE_MIN_FILTER,
				  GL_NEAREST);
	dispatch->glTexParameteri(GL_TEXTURE_2D,
				  GL_TEXTURE_MAG_FILTER,
				  GL_NEAREST);

	glamor_set_destination_pixmap_fbo(temp_fbo);
	dispatch->glUseProgram(glamor_priv->finish_access_prog[no_alpha]);
	dispatch->glUniform1i(glamor_priv->
			      finish_access_revert[no_alpha],
			      revert);
	dispatch->glUniform1i(glamor_priv->finish_access_swap_rb[no_alpha],
			      swap_rb);

	dispatch->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	dispatch->glDisableVertexAttribArray(GLAMOR_VERTEX_POS);
	dispatch->glDisableVertexAttribArray(GLAMOR_VERTEX_SOURCE);
	dispatch->glUseProgram(0);
	glamor_put_dispatch(glamor_priv);
	return temp_fbo;
}

/*
 * Download a sub region of pixmap to a specified memory region.
 * The pixmap must have a valid FBO, otherwise return a NULL.
 * */

void *
glamor_download_sub_pixmap_to_cpu(PixmapPtr pixmap, int x, int y, int w, int h,
				  int stride, void *bits, int pbo, glamor_access_t access)
{
	glamor_pixmap_private *pixmap_priv;
	GLenum format, type, gl_access, gl_usage;
	int no_alpha, revert, swap_rb;
	void *data, *read;
	ScreenPtr screen;
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(pixmap->drawable.pScreen);
	glamor_gl_dispatch *dispatch;
	glamor_pixmap_fbo *temp_fbo = NULL;
	int need_post_conversion = 0;
	int need_free_data = 0;

	data = bits;
	screen = pixmap->drawable.pScreen;
	pixmap_priv = glamor_get_pixmap_private(pixmap);
	if (!GLAMOR_PIXMAP_PRIV_HAS_FBO(pixmap_priv))
		return NULL;

	switch (access) {
	case GLAMOR_ACCESS_RO:
		gl_access = GL_READ_ONLY;
		gl_usage = GL_STREAM_READ;
		break;
	case GLAMOR_ACCESS_WO:
		return bits;
	case GLAMOR_ACCESS_RW:
		gl_access = GL_READ_WRITE;
		gl_usage = GL_DYNAMIC_DRAW;
		break;
	default:
		ErrorF("Glamor: Invalid access code. %d\n", access);
		assert(0);
	}

	if (glamor_get_tex_format_type_from_pixmap(pixmap,
						   &format,
						   &type,
						   &no_alpha,
						   &revert,
						   &swap_rb, 0)) {
		ErrorF("Unknown pixmap depth %d.\n",
		       pixmap->drawable.depth);
		assert(0);	// Should never happen.
		return NULL;
	}

	glamor_set_destination_pixmap_priv_nc(pixmap_priv);
	/* XXX we may don't need to validate it on GPU here,
	 * we can just validate it on CPU. */
	glamor_validate_pixmap(pixmap);

	need_post_conversion = (revert > REVERT_NORMAL);
	if (need_post_conversion) {
		if (pixmap->drawable.depth == 1) {
			int temp_stride;
			temp_stride = (((w * 8 + 7) / 8) + 3) & ~3;
			data = malloc(temp_stride * h);
			if (data == NULL)
				return NULL;
			need_free_data = 1;
		}
	}

	if (glamor_priv->gl_flavor == GLAMOR_GL_ES2
	    && !need_post_conversion
	    && (swap_rb != SWAP_NONE_DOWNLOADING || revert != REVERT_NONE)) {
		 if (!(temp_fbo = glamor_es2_pixmap_read_prepare(pixmap, x, y, w, h,
								 format, type, no_alpha,
								 revert, swap_rb))) {
			free(data);
			return NULL;
		}
		x = 0;
		y = 0;
	}

	dispatch = glamor_get_dispatch(glamor_priv);
	dispatch->glPixelStorei(GL_PACK_ALIGNMENT, 4);

	if (glamor_priv->has_pack_invert || glamor_priv->yInverted) {

		if (!glamor_priv->yInverted) {
			assert(glamor_priv->gl_flavor ==
			       GLAMOR_GL_DESKTOP);
			dispatch->glPixelStorei(GL_PACK_INVERT_MESA, 1);
		}

		if (glamor_priv->gl_flavor == GLAMOR_GL_DESKTOP && data == NULL) {
			assert(pbo > 0);
			dispatch->glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
			dispatch->glBufferData(GL_PIXEL_PACK_BUFFER,
					       stride *
					       h,
					       NULL, gl_usage);
		}

		dispatch->glReadPixels(x, y, w, h, format, type, data);

		if (!glamor_priv->yInverted) {
			assert(glamor_priv->gl_flavor == GLAMOR_GL_DESKTOP);
			dispatch->glPixelStorei(GL_PACK_INVERT_MESA, 0);
		}
		if (glamor_priv->gl_flavor == GLAMOR_GL_DESKTOP && bits == NULL) {
			bits = dispatch->glMapBuffer(GL_PIXEL_PACK_BUFFER,
						     gl_access);
			dispatch->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		}
	} else {
		int temp_pbo;
		int yy;

		dispatch = glamor_get_dispatch(glamor_priv);
		dispatch->glGenBuffers(1, &temp_pbo);
		dispatch->glBindBuffer(GL_PIXEL_PACK_BUFFER,
				       temp_pbo);
		dispatch->glBufferData(GL_PIXEL_PACK_BUFFER,
				       stride *
				       h,
				       NULL, GL_STREAM_READ);
		dispatch->glReadPixels(0, 0, w, h,
				       format, type, 0);
		read = dispatch->glMapBuffer(GL_PIXEL_PACK_BUFFER,
					     GL_READ_ONLY);
		for (yy = 0; yy < pixmap->drawable.height; yy++)
			memcpy(data + yy * stride,
			       read + (h - yy - 1) * stride, stride);
		dispatch->glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		dispatch->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		dispatch->glDeleteBuffers(1, &temp_pbo);
	}

	dispatch->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glamor_put_dispatch(glamor_priv);

	if (need_post_conversion) {
		/* As OpenGL desktop version never enters here.
		 * Don't need to consider if the pbo is valid.*/
		bits = glamor_color_convert_to_bits(data, bits,
						    w, h,
						    stride,
						    no_alpha,
						    revert, swap_rb);
	}

      done:

	if (temp_fbo != NULL)
		glamor_destroy_fbo(temp_fbo);
	if (need_free_data)
		free(data);

	return bits;
}


/**
 * Move a pixmap to CPU memory.
 * The input data is the pixmap's fbo.
 * The output data is at pixmap->devPrivate.ptr. We always use pbo
 * to read the fbo and then map it to va. If possible, we will use
 * it directly as devPrivate.ptr.
 * If successfully download a fbo to cpu then return TRUE.
 * Otherwise return FALSE.
 **/
Bool
glamor_download_pixmap_to_cpu(PixmapPtr pixmap, glamor_access_t access)
{
	glamor_pixmap_private *pixmap_priv =
	    glamor_get_pixmap_private(pixmap);
	unsigned int stride, y;
	GLenum format, type, gl_access, gl_usage;
	int no_alpha, revert, swap_rb;
	void *data = NULL, *dst;
	ScreenPtr screen;
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(pixmap->drawable.pScreen);
	glamor_gl_dispatch *dispatch;
	int pbo = 0;

	screen = pixmap->drawable.pScreen;
	if (!GLAMOR_PIXMAP_PRIV_HAS_FBO(pixmap_priv))
		return TRUE;

	glamor_debug_output(GLAMOR_DEBUG_TEXTURE_DOWNLOAD,
			    "Downloading pixmap %p  %dx%d depth%d\n",
			    pixmap,
			    pixmap->drawable.width,
			    pixmap->drawable.height,
			    pixmap->drawable.depth);

	stride = pixmap->devKind;

	if (access == GLAMOR_ACCESS_WO
	    || glamor_priv->gl_flavor == GLAMOR_GL_ES2
	    || (!glamor_priv->has_pack_invert && !glamor_priv->yInverted)) {
		data = malloc(stride * pixmap->drawable.height);
	} else {
		dispatch = glamor_get_dispatch(glamor_priv);
		if (pixmap_priv->fbo->pbo == 0)
			dispatch->glGenBuffers(1,
					       &pixmap_priv->fbo->pbo);
		glamor_put_dispatch(glamor_priv);
		pbo = pixmap_priv->fbo->pbo;
	}

	if (pixmap_priv->type == GLAMOR_TEXTURE_DRM) {
		stride = PixmapBytePad(pixmap->drawable.width, pixmap->drawable.depth);
		pixmap_priv->drm_stride = pixmap->devKind;
		pixmap->devKind = stride;
	}

	dst = glamor_download_sub_pixmap_to_cpu(pixmap, 0, 0,
						pixmap->drawable.width,
						pixmap->drawable.height,
						pixmap->devKind,
						data, pbo, access);

	if (!dst) {
		if (data)
			free(data);
		return FALSE;
	}

	if (pbo != 0)
		pixmap_priv->fbo->pbo_valid = 1;

	pixmap_priv->gl_fbo = GLAMOR_FBO_DOWNLOADED;

      done:

	pixmap->devPrivate.ptr = dst;

	return TRUE;
}

/* fixup a fbo to the exact size as the pixmap. */
Bool
glamor_fixup_pixmap_priv(ScreenPtr screen, glamor_pixmap_private *pixmap_priv)
{
	glamor_screen_private *glamor_priv;
	glamor_pixmap_fbo *old_fbo;
	glamor_pixmap_fbo *new_fbo = NULL;
	PixmapPtr scratch = NULL;
	glamor_pixmap_private *scratch_priv;
	DrawablePtr drawable;
	GCPtr gc = NULL;
	int ret = FALSE;

	drawable = &pixmap_priv->container->drawable;

	if (pixmap_priv->container->drawable.width == pixmap_priv->fbo->width
	    && pixmap_priv->container->drawable.height == pixmap_priv->fbo->height)
		return	TRUE;

	old_fbo = pixmap_priv->fbo;
	glamor_priv = pixmap_priv->glamor_priv;

	if (!old_fbo)
		return FALSE;

	gc = GetScratchGC(drawable->depth, screen);
	if (!gc)
		goto fail;

	scratch = glamor_create_pixmap(screen, drawable->width, drawable->height,
				       drawable->depth,
				       GLAMOR_CREATE_PIXMAP_FIXUP);

	scratch_priv = glamor_get_pixmap_private(scratch);

	if (!scratch_priv || !scratch_priv->fbo)
		goto fail;

	ValidateGC(&scratch->drawable, gc);
	glamor_copy_area(drawable,
			 &scratch->drawable,
			 gc, 0, 0,
			 drawable->width, drawable->height,
			 0, 0);
	old_fbo = glamor_pixmap_detach_fbo(pixmap_priv);
	new_fbo = glamor_pixmap_detach_fbo(scratch_priv);
	glamor_pixmap_attach_fbo(pixmap_priv->container, new_fbo);
	glamor_pixmap_attach_fbo(scratch, old_fbo);

	DEBUGF("old %dx%d type %d\n",
		drawable->width, drawable->height, pixmap_priv->type);
	DEBUGF("copy tex %d  %dx%d to tex %d %dx%d \n",
		old_fbo->tex, old_fbo->width, old_fbo->height, new_fbo->tex, new_fbo->width, new_fbo->height);
	ret = TRUE;
fail:
	if (gc)
		FreeScratchGC(gc);
	if (scratch)
		glamor_destroy_pixmap(scratch);

	return ret;
}

/*
 * We may use this function to reduce a large pixmap to a small sub
 * pixmap. Two scenarios currently:
 * 1. When fallback a large textured pixmap to CPU but we do need to
 * do rendering within a small sub region, then we can just get a
 * sub region.
 *
 * 2. When uploading a large pixmap to texture but we only need to
 * use part of the source/mask picture. As glTexImage2D will be more
 * efficient to upload a contingent region rather than a sub block
 * in a large buffer. We use this function to gather the sub region
 * to a contingent sub pixmap.
 *
 * The sub-pixmap must have the same format as the source pixmap.
 *
 * */
PixmapPtr
glamor_get_sub_pixmap(PixmapPtr pixmap, int x, int y, int w, int h, glamor_access_t access)
{
	glamor_screen_private *glamor_priv;
	PixmapPtr sub_pixmap;
	glamor_pixmap_private *sub_pixmap_priv, *pixmap_priv;
	void *data;
	int pbo;
	int flag;

	if (access == GLAMOR_ACCESS_WO) {
		sub_pixmap = glamor_create_pixmap(pixmap->drawable.pScreen, w, h,
						  pixmap->drawable.depth, GLAMOR_CREATE_PIXMAP_CPU);
		ErrorF("WO\n");
		return sub_pixmap;
	}

	glamor_priv = glamor_get_screen_private(pixmap->drawable.pScreen);
	pixmap_priv = glamor_get_pixmap_private(pixmap);

	if (!GLAMOR_PIXMAP_PRIV_HAS_FBO(pixmap_priv))
		return NULL;
	if (glamor_priv->gl_flavor == GLAMOR_GL_ES2)
		flag = GLAMOR_CREATE_PIXMAP_CPU;
	else
		flag = GLAMOR_CREATE_PIXMAP_MAP;

	sub_pixmap = glamor_create_pixmap(pixmap->drawable.pScreen, w, h,
					  pixmap->drawable.depth, flag);

	if (sub_pixmap == NULL)
		return NULL;

	sub_pixmap_priv = glamor_get_pixmap_private(sub_pixmap);
	pbo = sub_pixmap_priv ? (sub_pixmap_priv->fbo ? sub_pixmap_priv->fbo->pbo : 0): 0;

	if (pixmap_priv->is_picture) {
		sub_pixmap_priv->pict_format = pixmap_priv->pict_format;
		sub_pixmap_priv->is_picture = pixmap_priv->is_picture;
	}

	if (pbo)
		data = NULL;
	else {
		data = sub_pixmap->devPrivate.ptr;
		assert(flag != GLAMOR_CREATE_PIXMAP_MAP);
	}
	data = glamor_download_sub_pixmap_to_cpu(pixmap, x, y, w, h, sub_pixmap->devKind,
						 data, pbo, access);
	if (pbo) {
		assert(sub_pixmap->devPrivate.ptr == NULL);
		sub_pixmap->devPrivate.ptr = data;
		sub_pixmap_priv->fbo->pbo_valid = 1;
	}
#if 0
	struct pixman_box16 box;
	PixmapPtr new_sub_pixmap;
	int dx, dy;
	box.x1 = 0;
	box.y1 = 0;
	box.x2 = w;
	box.y2 = h;

	dx = x;
	dy = y;

	new_sub_pixmap = glamor_create_pixmap(pixmap->drawable.pScreen, w, h,
					      pixmap->drawable.depth, GLAMOR_CREATE_PIXMAP_CPU);
	glamor_copy_n_to_n(&pixmap->drawable, &new_sub_pixmap, NULL, &box, 1, dx, dy, 0, 0, 0, NULL);
	glamor_compare_pixmaps(new_sub_pixmap, sub_pixmap, 0, 0, w, h, 1, 1);
#endif

	return sub_pixmap;
}

PixmapPtr
glamor_put_sub_pixmap(PixmapPtr sub_pixmap, PixmapPtr pixmap, int x, int y, int w, int h, glamor_access_t access)
{
	void *bits;
	int pbo;
	glamor_pixmap_private *sub_pixmap_priv;
	if (access != GLAMOR_ACCESS_RO) {
		sub_pixmap_priv = glamor_get_pixmap_private(sub_pixmap);
		if (sub_pixmap_priv
		    && sub_pixmap_priv->fbo
		    && sub_pixmap_priv->fbo->pbo_valid) {
			bits = NULL;
			pbo = sub_pixmap_priv->fbo->pbo;
		} else {
			bits = sub_pixmap->devPrivate.ptr;
			pbo = 0;
		}
		glamor_upload_sub_pixmap_to_texture(pixmap, x, y, w, h, sub_pixmap->devKind, bits, pbo);
	}
	glamor_destroy_pixmap(sub_pixmap);
}
