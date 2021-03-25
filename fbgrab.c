/*
 * fbgrab - takes screenshots using the framebuffer.
 *
 * (C) Gunnar Monell <gunnar.monell@gmail.com> 2002-2021
 *
 * This program is free Software, see the COPYING file
 * and is based on Stephan Beyer's <fbshot@s-beyer.de> FBShot
 * (C) 2000.
 *
 * For features and differences, read the manual page.
 *
 * This program has been checked with "splint +posixlib" without
 * warnings. Splint is available from http://www.splint.org/ .
 * Patches and enhancements of fbgrab have to fulfill this too.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <getopt.h>
#include <sys/vt.h>   /* to handle vt changing */
#ifdef S_SPLINT_S
#define Z_BEST_COMPRESSION 1
#define Z_DEFAULT_COMPRESSION 0
#define png_int_32 int
#define png_byte char
#define png_uint_32 unsigned
#define png_int_16 short
#define png_uint_16 unsigned
#define __signed__ signed
#else
#include <zlib.h>
#endif
#include <png.h>      /* PNG lib */
#include <linux/fb.h> /* to handle framebuffer ioctls */

#define	VERSION	"1.5"
#define	DEFAULT_FB	"/dev/fb0"
#define MAX_LEN 512
#define UNDEFINED -1

static int srcBlue = 0;
static int srcGreen = 1;
static int srcRed = 2;
static int srcAlpha = 3;

static const __u32 Blue = 0;
static const __u32 Green = 1;
static const __u32 Red = 2;
static const __u32 Alpha = 3;

/*@noreturn@*/ static void fatal_error(char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void usage(char *binary)
{
    fprintf(stderr, "Usage:   %s\t[-hi] [-{C|c} vt] [-d dev] [-s n] [-z n]\n"
	   "\t\t[-f fromfile -w n -h n -b n] filename.png\n", binary);
}

static void help(char *binary)
{
    fprintf(stderr, "fbgrab - takes screenshots using the framebuffer, v%s\n", VERSION);

    usage(binary);

    fprintf(stderr, "\nPossible options:\n");
/* please keep this list alphabetical */
    fprintf(stderr, "\t-a    \tignore the alpha channel, to support pixel formats like BGR32\n");
    fprintf(stderr, "\t-b n  \tforce use of n bits/pixel, required when reading from file\n");
    fprintf(stderr, "\t-C n  \tgrab from console n, for slower framebuffers\n");
    fprintf(stderr, "\t-c n  \tgrab from console n\n");
    fprintf(stderr, "\t-d dev\tuse framebuffer device dev instead of default\n");
    fprintf(stderr, "\t-f file\t read from file instead of framebuffer\n");
    fprintf(stderr, "\t-h n  \tset height to n pixels, required when reading from file\n"
	   "\t\tcan be used to force height when reading from framebuffer\n");
    fprintf(stderr, "\t-i    \tturns on interlacing in PNG\n");
    fprintf(stderr, "\t-s n  \tsleep n seconds before making screenshot\n");
    fprintf(stderr, "\t-v    \tverbose, print debug information.\n");
    fprintf(stderr, "\t-w n  \tset width to n pixels, required when reading from file\n"
	   "\t\tcan be used to force width when reading from framebuffer\n");
    fprintf(stderr, "\t-l n  \tset line length, stride, to n pixels, required when reading from file\n");
    fprintf(stderr, "\t-z n  \tPNG compression level: 0 (fast) .. 9 (best)\n");
    fprintf(stderr, "\t-?    \tprint this usage information\n");
}


static void chvt(int num)
{
    int fd;

    if(-1 == (fd = open("/dev/console", O_RDWR)))
	fatal_error("Cannot open /dev/console");

    if (ioctl(fd, VT_ACTIVATE, num) != 0)
	fatal_error("ioctl VT_ACTIVATE");

    if (ioctl(fd, VT_WAITACTIVE, num) != 0)
	fatal_error("ioctl VT_WAITACTIVE");

    (void) close(fd);
}

static unsigned short int change_to_vt(unsigned short int vt_num)
{
    int fd;
    unsigned short int old_vt;
    struct vt_stat vt_info;

    memset(&vt_info, 0, sizeof(struct vt_stat));

    if(-1 == (fd=open("/dev/console", O_RDONLY)))
	fatal_error("Couldn't open /dev/console");

    if (ioctl(fd, VT_GETSTATE,  &vt_info) != 0)
	fatal_error("ioctl VT_GETSTATE");

    (void) close (fd);

    old_vt = vt_info.v_active;

    chvt((int) vt_num); /* go there for information */

    return old_vt;
}

static void get_framebufferdata(char *device, struct fb_var_screeninfo *fb_varinfo_p, struct fb_fix_screeninfo *fb_fixedinfo, int verbose)
{
    int fd;

    /* now open framebuffer device */
    if(-1 == (fd=open(device, O_RDONLY)))
    {
	fprintf(stderr, "Error: Couldn't open %s.\n", device);
	exit(EXIT_FAILURE);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, fb_varinfo_p) != 0)
	fatal_error("ioctl FBIOGET_VSCREENINFO");

    if (ioctl(fd, FBIOGET_FSCREENINFO, fb_fixedinfo) != 0)
	fatal_error("ioctl FBIOGET_FSCREENINFO");

    if (0!=verbose)
    {
        fprintf(stderr, "frame buffer fixed info:\n");
        fprintf(stderr, "id: \"%s\"\n", fb_fixedinfo->id);
    	switch (fb_fixedinfo->type)
        {
    	case FB_TYPE_PACKED_PIXELS:
    		fprintf(stderr, "type: packed pixels\n");
    		break;
    	case FB_TYPE_PLANES:
	    	fprintf(stderr, "type: non interleaved planes\n");
		    break;
    	case FB_TYPE_INTERLEAVED_PLANES:
	    	fprintf(stderr, "type: interleaved planes\n");
		    break;
    	case FB_TYPE_TEXT:
    		fprintf(stderr, "type: text/attributes\n");
            break;
    	case FB_TYPE_VGA_PLANES:
    		fprintf(stderr, "type: EGA/VGA planes\n");
		    break;
    	default:
    		fprintf(stderr, "type: undefined!\n");
	    	break;
    	}
        fprintf(stderr, "line length: %i bytes (%i pixels)\n", (int) fb_fixedinfo->line_length, (int) fb_fixedinfo->line_length/(fb_varinfo_p->bits_per_pixel/8));

        fprintf(stderr, "\nframe buffer variable info:\n");
        fprintf(stderr, "resolution: %ix%i\n", (int) fb_varinfo_p->xres, (int) fb_varinfo_p->yres);
        fprintf(stderr, "virtual resolution: %ix%i\n", (int) fb_varinfo_p->xres_virtual, (int) fb_varinfo_p->yres_virtual);
        fprintf(stderr, "offset: %ix%i\n", (int) fb_varinfo_p->xoffset, (int) fb_varinfo_p->yoffset);
        fprintf(stderr, "bits_per_pixel: %i\n", (int) fb_varinfo_p->bits_per_pixel);
        fprintf(stderr, "grayscale: %s\n", fb_varinfo_p->grayscale!=0 ? "true" : "false");
        fprintf(stderr, "red:   offset: %i, length: %i, msb_right: %i\n", (int) fb_varinfo_p->red.offset, (int) fb_varinfo_p->red.length, (int) fb_varinfo_p->red.msb_right);
        fprintf(stderr, "green: offset: %i, length: %i, msb_right: %i\n", (int) fb_varinfo_p->green.offset, (int) fb_varinfo_p->green.length, (int) fb_varinfo_p->green.msb_right);
        fprintf(stderr, "blue:  offset: %i, length: %i, msb_right: %i\n", (int) fb_varinfo_p->blue.offset, (int) fb_varinfo_p->blue.length, (int) fb_varinfo_p->blue.msb_right);
        fprintf(stderr, "alpha: offset: %i, length: %i, msb_right: %i\n", (int) fb_varinfo_p->transp.offset, (int) fb_varinfo_p->transp.length, (int) fb_varinfo_p->transp.msb_right);
        fprintf(stderr, "pixel format: %s\n", fb_varinfo_p->nonstd == 0 ? "standard" : "non-standard");
    }
    srcBlue = (int) (fb_varinfo_p->blue.offset >> 3);
    srcGreen = (int) (fb_varinfo_p->green.offset >> 3);
    srcRed = (int) (fb_varinfo_p->red.offset >> 3);

    (void) close(fd);
}

static void read_framebuffer(char *device, size_t bytes, unsigned char *buf_p, int skip_bytes)
{
    int fd;

    if(-1 == (fd=open(device, O_RDONLY)))
    {
	fprintf(stderr, "Error: Couldn't open %s.\n", device);
	exit(EXIT_FAILURE);
    }

    if (skip_bytes!=0) {
        if (-1 == lseek(fd, skip_bytes, SEEK_SET))
    	    fatal_error("Error: Could not seek to framebuffer start position.\n");
    }

    if (buf_p == NULL || read(fd, buf_p, bytes) != (ssize_t) bytes)
	    fatal_error("Error: Not enough memory or data\n");
}

static void convert1555to32(unsigned int width, unsigned int height,
                int line_length,
			    unsigned char *inbuffer,
			    unsigned char *outbuffer)
{
    unsigned int row;
    unsigned int col;
    for (row=0; row < height; row++)
    for (col=0; col < (unsigned int) width; col++)
    {
        unsigned int srcidx = 2 * (row * line_length + col);
        unsigned int dstidx = 4 * (row * width + col);
	/* BLUE  = 0 */
	    outbuffer[dstidx+Blue] = (inbuffer[srcidx+1] & 0x7C) << 1;
	/* GREEN = 1 */
        outbuffer[dstidx+Green] = (((inbuffer[srcidx+1] & 0x3) << 3) |
			     ((inbuffer[srcidx] & 0xE0) >> 5)) << 3;
	/* RED   = 2 */
	    outbuffer[dstidx+Red] = (inbuffer[srcidx] & 0x1f) << 3;
	/* ALPHA = 3 */
	    outbuffer[dstidx+Alpha] = '\0';
    }
}

static void convert565to32(unsigned int width, unsigned int height,
                int line_length,
		        unsigned char *inbuffer,
			    unsigned char *outbuffer)
{
    unsigned int row;
    unsigned int col;
    for (row=0; row < height; row++)
    for (col=0; col < (unsigned int) width; col++)
    {
        unsigned int srcidx = 2 * (row * line_length + col);
        unsigned int dstidx = 4 * (row * width + col);
        /* BLUE  = 0 */
        outbuffer[dstidx+Blue] = (inbuffer[srcidx] & 0x1f) << 3;
        /* GREEN = 1 */
        outbuffer[dstidx+Green] = (((inbuffer[srcidx+1] & 0x7) << 3) |
                                (inbuffer[srcidx] & 0xE0) >> 5) << 2;
        /* RED   = 2 */
        outbuffer[dstidx+Red] = (inbuffer[srcidx+1] & 0xF8);
        /* ALPHA = 3 */
        outbuffer[dstidx+Alpha] = '\0';
    }
}

static void convert888to32(unsigned int width, unsigned int height,
                int line_length,
                unsigned char *inbuffer,
 			    unsigned char *outbuffer)
{
    unsigned int row;
    unsigned int col;
    for (row=0; row<height; row++)
    for (col=0; col < (unsigned int) width; col++)
    {
        unsigned int srcidx = 3 * (row * line_length + col);
        unsigned int dstidx = 4 * (row * width + col);
    /* BLUE  = 0 */
	    outbuffer[dstidx+Blue] = inbuffer[srcidx+srcBlue];
	/* GREEN = 1 */
        outbuffer[dstidx+Green] = inbuffer[srcidx+srcGreen];
	/* RED   = 2 */
        outbuffer[dstidx+Red] = inbuffer[srcidx+srcRed];
	/* ALPHA */
        outbuffer[dstidx+Alpha] = '\0';
    }
}

static void convert8888to32(unsigned int width, unsigned int height,
                int line_length,
                unsigned char *inbuffer,
			    unsigned char *outbuffer)
{
    unsigned int row;
    unsigned int col;
    for (row=0; row<height; row++)
    for (col=0; col < (unsigned int) width; col++)
    {
        unsigned int srcidx = 4 * (row * line_length + col);
        unsigned int dstidx = 4 * (row * width + col);
	/* BLUE  = 0 */
    	outbuffer[dstidx+Blue] = inbuffer[srcidx+srcBlue];
	/* GREEN = 1 */
        outbuffer[dstidx+Green] = inbuffer[srcidx+srcGreen];
	/* RED   = 2 */
        outbuffer[dstidx+Red] = inbuffer[srcidx+srcRed];
	/* ALPHA */
        outbuffer[dstidx+Alpha] = srcAlpha >= 0 ? inbuffer[srcidx+srcAlpha] : '\0';
    }
}


static void write_PNG(unsigned char *outbuffer, char *filename,
				unsigned int width, unsigned int height, int interlace, int compression)
{
    unsigned int i;
    int bit_depth=0, color_type;
    png_bytep row_pointers[height];
    png_structp png_ptr;
    png_infop info_ptr;
    FILE *outfile;

    memset((void*) row_pointers, (int) sizeof(png_bytep), (size_t) height);

    if (strcmp(filename, "-") == 0)
        outfile = stdout;
    else {
        outfile = fopen(filename, "wb");
        if (!outfile)
        {
            fprintf(stderr, "Error: Couldn't fopen %s.\n", filename);
            exit(EXIT_FAILURE);
        }
    }

    for (i=0; i<height; i++)
	row_pointers[i] = (png_bytep) &outbuffer[i * 4 * width];

    /*@-nullpass*/
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	(png_voidp) NULL, (png_error_ptr) NULL, (png_error_ptr) NULL);

    if (!png_ptr)
	fatal_error("Error: Couldn't create PNG write struct.");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
	png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
	fatal_error("Error: Couldn't create PNG info struct.");
    }
    /*@+nullpass*/

    png_init_io(png_ptr, outfile);

    png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

    bit_depth = 8;
    color_type = PNG_COLOR_TYPE_RGB_ALPHA;
    png_set_invert_alpha(png_ptr);
    png_set_bgr(png_ptr);

    png_set_IHDR(png_ptr, info_ptr, width, height,
		 bit_depth, color_type, interlace,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    fprintf(stderr, "Now writing PNG file (compression %d)\n", compression);

    png_write_image(png_ptr, row_pointers);

    png_write_end(png_ptr, info_ptr);
    /* puh, done, now freeing memory... */
    png_destroy_write_struct(&png_ptr, &info_ptr);

    if (outfile != NULL)
	(void) fclose(outfile);
    /*@i2@*/ } /* tell splint to ignore false warning for not
		  released memory of png_ptr and info_ptr */

static void convert_and_write(unsigned char *inbuffer, char *filename,
				unsigned int width, unsigned int height, int line_length, int bits, int interlace, int compression)
{
    size_t bufsize = (size_t) line_length * height * 4;

    unsigned char *outbuffer = (unsigned char*) malloc(bufsize);

    if (outbuffer == NULL)
	fatal_error("Not enough memory");

    memset(outbuffer, 0, bufsize);

    fprintf(stderr, "Converting image from %i\n", bits);

    switch(bits)
    {
    case 15:
    	convert1555to32(width, height, line_length, inbuffer, outbuffer);
    	break;
    case 16:
        convert565to32(width, height, line_length, inbuffer, outbuffer);
        break;
    case 24:
        convert888to32(width, height, line_length, inbuffer, outbuffer);
        break;
    case 32:
        convert8888to32(width, height, line_length, inbuffer, outbuffer);
        break;
    default:
        fprintf(stderr, "%d bits per pixel are not supported! ", bits);
        exit(EXIT_FAILURE);
    }

    write_PNG(outbuffer, filename, width, height, interlace, compression);
    (void) free(outbuffer);
}


/********
 * MAIN *
 ********/

int main(int argc, char **argv)
{
    unsigned char *buf_p;
    char *device = NULL;
    char *outfile = argv[argc-1];
    int optc;
    int ignore_alpha=UNDEFINED, vt_num=UNDEFINED, bitdepth=UNDEFINED, height=UNDEFINED, width=UNDEFINED;
    int old_vt=UNDEFINED;
    int line_length = UNDEFINED;
    size_t buf_size;
    char infile[MAX_LEN];
    struct fb_var_screeninfo fb_varinfo;
    struct fb_fix_screeninfo fb_fixedinfo;
    int waitbfg=0; /* wait before grabbing (for -C )... */
    int interlace = PNG_INTERLACE_NONE;
    int verbose = 0;
    int png_compression = Z_DEFAULT_COMPRESSION;
    int skip_bytes = 0;

    memset(infile, 0, MAX_LEN);
    memset(&fb_varinfo, 0, sizeof(struct fb_var_screeninfo));
    memset(&fb_fixedinfo, 0, sizeof(struct fb_fix_screeninfo));

    for(;;)
    {
	optc=getopt(argc, argv, "f:z:w:b:h:l:iC:c:d:s:?av");
	if (optc==-1)
	    break;
	switch (optc)
	{
/* please keep this list alphabetical */
	case 'a':
	    ignore_alpha = 1;
	    break;
	case 'b':
	    bitdepth = atoi(optarg);
	    break;
	case 'C':
	    waitbfg = 1;
	    /*@fallthrough@*/
	case 'c':
	    vt_num = atoi(optarg);
	    break;
	case 'd':
	    device = optarg;
	    break;
	case 'f':
	    strncpy(infile, optarg, MAX_LEN);
	    break;
	case 'h':
	    height = atoi(optarg);
	    break;
	case '?':
	    help(argv[0]);
	    return 1;
	case 'i':
	    interlace = PNG_INTERLACE_ADAM7;
	    break;
	case 'l':
	    line_length = atoi(optarg);
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 's':
	    (void) sleep((unsigned int) atoi(optarg));
	    break;
	case 'w':
	    width = atoi(optarg);
	    break;
	case 'z':
	    png_compression = atoi(optarg);
	    break;
	default:
	    usage(argv[0]);
	}
    }

    if ((optind==argc) || (1!=argc-optind))
    {
        usage(argv[0]);
        return 1;
    }

    if (UNDEFINED != vt_num)
    {
	old_vt = (int) change_to_vt((unsigned short int) vt_num);
	if (waitbfg != 0) (void) sleep(3);
    }

    if (strlen(infile) > 0)
    {
        if (UNDEFINED == bitdepth || UNDEFINED == width || UNDEFINED == height)
        {
            fprintf(stderr, "Width, height and bitdepth are mandatory when reading from file\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        if (NULL == device)
        {
            device = getenv("FRAMEBUFFER");
            if (NULL == device)
            {
                device = DEFAULT_FB;
            }
    	}

        get_framebufferdata(device, &fb_varinfo, &fb_fixedinfo, verbose);

        if ((UNDEFINED == ignore_alpha) && (fb_varinfo.transp.length > 0)) {
            srcAlpha = (int) (fb_varinfo.transp.offset >> 3);
        } else {
            srcAlpha = -1; // not used
        }

        if (UNDEFINED == bitdepth)
            bitdepth = (int) fb_varinfo.bits_per_pixel;

        if (UNDEFINED == width)
            width = (int) fb_varinfo.xres;

        if (UNDEFINED == height)
            height = (int) fb_varinfo.yres;

        if (UNDEFINED == line_length)
            line_length = (int) fb_fixedinfo.line_length/(fb_varinfo.bits_per_pixel>>3);

        skip_bytes =  (int) ((fb_varinfo.yoffset * fb_varinfo.xres) * (fb_varinfo.bits_per_pixel >> 3));

        fprintf(stderr, "Resolution: %ix%i depth %i\n", width, height, bitdepth);

        strncpy(infile, device, MAX_LEN - 1);
    }

    buf_size = (size_t) (line_length * height * (((unsigned int) bitdepth + 7) >> 3));

    buf_p = (unsigned char*) malloc(buf_size);

    if(buf_p == NULL)
        fatal_error("Not enough memory");

    if(line_length < width)
        fatal_error("Line length cannot be smaller than width");

    memset(buf_p, 0, buf_size);

    read_framebuffer(infile, buf_size, buf_p, skip_bytes);

    if (UNDEFINED != old_vt)
    (void) change_to_vt((unsigned short int) old_vt);

    convert_and_write(buf_p, outfile, (unsigned int) width, (unsigned int) height, line_length, bitdepth, interlace, png_compression);

    (void) free(buf_p);

    return 0;
}
