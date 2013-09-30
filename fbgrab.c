/*
 * fbgrab - takes screenshots using the framebuffer.
 *
 * (C) Gunnar Monell <gmo@linux.nu> 2002
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

#include <getopt.h>
#include <sys/vt.h>   /* to handle vt changing */
#include <png.h>      /* PNG lib */
#include <zlib.h>
#include <linux/fb.h> /* to handle framebuffer ioctls */

#define	VERSION	"1.0 beta 1"
#define	DEFAULT_FB	"/dev/fb0"
#define MAX_LEN 512
#define UNDEFINED -1

static int Blue = 0;
static int Green = 1;
static int Red = 2;
static int Alpha = 3;

/*@noreturn@*/ static void fatal_error(char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void usage(char *binary)
{
    printf("Usage:   %s\t[-hi] [-{C|c} vt] [-d dev] [-s n]\n"
	   "\t\t[-f fromfile -w n -h n -b n] filename.png\n", binary);
}

static void help(char *binary)
{
    printf("fbgrab - takes screenshots using the framebuffer, v%s\n", VERSION);
     
    usage(binary);
    
    printf("\nPossible options:\n");
/* please keep this list alphabetical */
    printf("\t-b n  \tforce use of n bits/pixel, required when reading from file\n");
    printf("\t-C n  \tgrab from console n, for slower framebuffers\n");
    printf("\t-c n  \tgrab from console n\n");
    printf("\t-d dev\tuse framebuffer device dev instead of default\n");
    printf("\t-f file\t read from file instead of framebuffer\n");
    printf("\t-h n  \tset height to n pixels, required when reading from file\n"
	   "\t\tcan be used to force height when reading from framebuffer\n");
    printf("\t-i    \tturns on interlacing in PNG\n");
    printf("\t-s n  \tsleep n seconds before making screenshot\n");
    printf("\t-v    \tverbose, print debug information.\n");
    printf("\t-w n  \tset width to n pixels, required when reading from file\n"
	   "\t\tcan be used to force width when reading from framebuffer\n");
    printf("\t-?    \tprint this usage information\n");
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

static void get_framebufferdata(char *device, struct fb_var_screeninfo *fb_varinfo_p, int verbose)
{
    int fd;
    struct fb_fix_screeninfo fb_fixedinfo;
    
    /* now open framebuffer device */
    if(-1 == (fd=open(device, O_RDONLY)))
    {
	fprintf (stderr, "Error: Couldn't open %s.\n", device);
	exit(EXIT_FAILURE);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, fb_varinfo_p) != 0)
	fatal_error("ioctl FBIOGET_VSCREENINFO");

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_fixedinfo) != 0)
	fatal_error("ioctl FBIOGET_FSCREENINFO");

    if (verbose)
    {
    	printf("frame buffer fixed info:\n");
	printf("id: \"%s\"\n", fb_fixedinfo.id);
    	switch (fb_fixedinfo.type) 
    	{
    	case FB_TYPE_PACKED_PIXELS:
		printf("type: packed pixels\n");
		break;
    	case FB_TYPE_PLANES:
		printf("type: non interleaved planes\n");
		break;
    	case FB_TYPE_INTERLEAVED_PLANES:
		printf("type: interleaved planes\n");
		break;
    	case FB_TYPE_TEXT:
		printf("type: text/attributes\n");
		break;	
    	case FB_TYPE_VGA_PLANES:
		printf("type: EGA/VGA planes\n");
		break;
    	default:
		printf("type: undefined!\n");
		break;
    	}
	printf("line length: %i bytes (%i pixels)\n", fb_fixedinfo.line_length, fb_fixedinfo.line_length/(fb_varinfo_p->bits_per_pixel/8));
    
	printf("\nframe buffer variable info:\n");
	printf("resolution: %ix%i\n", fb_varinfo_p->xres, fb_varinfo_p->yres);
	printf("virtual resolution: %ix%i\n", fb_varinfo_p->xres_virtual, fb_varinfo_p->yres_virtual);
    	printf("offset: %ix%i\n", fb_varinfo_p->xoffset, fb_varinfo_p->yoffset);
    	printf("bits_per_pixel: %i\n", fb_varinfo_p->bits_per_pixel);
    	printf("grayscale: %s\n", fb_varinfo_p->grayscale ? "true" : "false");
    	printf("red:   offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->red.offset, fb_varinfo_p->red.length, fb_varinfo_p->red.msb_right);
    	printf("blue:  offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->blue.offset, fb_varinfo_p->green.length, fb_varinfo_p->green.msb_right);
    	printf("green: offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->green.offset, fb_varinfo_p->blue.length, fb_varinfo_p->blue.msb_right);
    	printf("alpha: offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->transp.offset, fb_varinfo_p->transp.length, fb_varinfo_p->transp.msb_right);
    	printf("pixel format: %s\n", fb_varinfo_p->nonstd == 0 ? "standard" : "non-standard");
    }
    Blue = fb_varinfo_p->blue.offset >> 3;
    Green = fb_varinfo_p->green.offset >> 3;
    Red = fb_varinfo_p->red.offset >> 3;
    Alpha = fb_varinfo_p->transp.offset >> 3;

    (void) close(fd);
}

static void read_framebuffer(char *device, size_t bytes, unsigned char *buf_p)
{
    int fd; 

    if(-1 == (fd=open(device, O_RDONLY)))
    {
	fprintf (stderr, "Error: Couldn't open %s.\n", device);
	exit(EXIT_FAILURE);
    }

    if (buf_p == NULL || read(fd, buf_p, bytes) != (ssize_t) bytes) 
	fatal_error("Error: Not enough memory or data\n");
}

static void convert1555to32(int width, int height, 
			    unsigned char *inbuffer, 
			    unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width*2; i+=2)
    {
	/* BLUE  = 0 */
	outbuffer[(i<<1)+Blue] = (inbuffer[i+1] & 0x7C) << 1;
	/* GREEN = 1 */
        outbuffer[(i<<1)+Green] = (((inbuffer[i+1] & 0x3) << 3) | 
			     ((inbuffer[i] & 0xE0) >> 5)) << 3;
	/* RED   = 2 */
	outbuffer[(i<<1)+Red] = (inbuffer[i] & 0x1f) << 3;
	/* ALPHA = 3 */
	outbuffer[(i<<1)+Alpha] = '\0'; 
    }
}

static void convert565to32(int width, int height, 
			   unsigned char *inbuffer, 
			   unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width*2; i+=2)
    {
	/* BLUE  = 0 */
	outbuffer[(i<<1)+Blue] = (inbuffer[i] & 0x1f) << 3;
	/* GREEN = 1 */
        outbuffer[(i<<1)+Green] = (((inbuffer[i+1] & 0x7) << 3) | 
			     (inbuffer[i] & 0xE0) >> 5) << 2;	
        /* RED   = 2 */
	outbuffer[(i<<1)+Red] = (inbuffer[i+1] & 0xF8);
	/* ALPHA = 3 */
	outbuffer[(i<<1)+Alpha] = '\0'; 
    }
}

static void convert888to32(int width, int height, 
			   unsigned char *inbuffer, 
			   unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width; i++)
    {
	/* BLUE  = 0 */
	outbuffer[(i<<2)+Blue] = inbuffer[i*3+0];
	/* GREEN = 1 */
        outbuffer[(i<<2)+Green] = inbuffer[i*3+1];
	/* RED   = 2 */
        outbuffer[(i<<2)+Red] = inbuffer[i*3+2];
	/* ALPHA */
        outbuffer[(i<<2)+Alpha] = '\0';
    }
}

static void convert8888to32(int width, int height, 
			   unsigned char *inbuffer, 
			   unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width; i++)
    {
	/* BLUE  = 0 */
	outbuffer[(i<<2)+Blue] = inbuffer[i*4+Blue];
	/* GREEN = 1 */
        outbuffer[(i<<2)+Green] = inbuffer[i*4+Green];
	/* RED   = 2 */
        outbuffer[(i<<2)+Red] = inbuffer[i*4+Red];
	/* ALPHA */
        outbuffer[(i<<2)+Alpha] = inbuffer[i*4+Alpha];
    }
}


static void write_PNG(unsigned char *outbuffer, char *filename, 
				int width, int height, int interlace)
{
    int i;
    int bit_depth=0, color_type;
    png_bytep row_pointers[height];
    png_structp png_ptr;
    png_infop info_ptr;
    FILE *outfile = fopen(filename, "wb");

    for (i=0; i<height; i++)
	row_pointers[i] = outbuffer + i * 4 * width;
    
    if (!outfile)
    {
	fprintf (stderr, "Error: Couldn't fopen %s.\n", filename);
	exit(EXIT_FAILURE);
    }
    
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
    
    printf ("Now writing PNG file\n");
    
    png_write_image(png_ptr, row_pointers);
    
    png_write_end(png_ptr, info_ptr);
    /* puh, done, now freeing memory... */
    png_destroy_write_struct(&png_ptr, &info_ptr);
    
    if (outfile != NULL)
	(void) fclose(outfile);
    /*@i2@*/ } /* tell splint to ignore false warning for not 
		  released memory of png_ptr and info_ptr */

static void convert_and_write(unsigned char *inbuffer, char *filename, 
				int width, int height, int bits, int interlace)
{
    size_t bufsize = (size_t) width * height * 4;

    unsigned char *outbuffer = malloc(bufsize);

    if (outbuffer == NULL)
	fatal_error("Not enough memory");
    
    memset(outbuffer, 0, bufsize);

    printf ("Converting image from %i\n", bits);
    
    switch(bits) 
    {
    case 15:
	convert1555to32(width, height, inbuffer, outbuffer);
	write_PNG(outbuffer, filename, width, height, interlace);
	break;
    case 16:
	convert565to32(width, height, inbuffer, outbuffer);
	write_PNG(outbuffer, filename, width, height, interlace);
	break;
    case 24:
	convert888to32(width, height, inbuffer, outbuffer);
	write_PNG(outbuffer, filename, width, height, interlace);
	break;
    case 32:
	convert8888to32(width, height, inbuffer, outbuffer);
	write_PNG(outbuffer, filename, width, height, interlace);
	break;
    default:
	fprintf(stderr, "%d bits per pixel are not supported! ", bits);
	exit(EXIT_FAILURE);
    }
        
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
    int vt_num=UNDEFINED, bitdepth=UNDEFINED, height=UNDEFINED, width=UNDEFINED;
    int old_vt=UNDEFINED;
    size_t buf_size;
    char infile[MAX_LEN];
    struct fb_var_screeninfo fb_varinfo;
    int waitbfg=0; /* wait before grabbing (for -C )... */
    int interlace = PNG_INTERLACE_NONE;
    int verbose = 0;

    memset(infile, 0, MAX_LEN);
    memset(&fb_varinfo, 0, sizeof(struct fb_var_screeninfo));


    for(;;)
    {
	optc=getopt(argc, argv, "f:w:b:gh:iC:c:d:s:?:v");
	if (optc==-1)
	    break;
	switch (optc) 
	{
/* please keep this list alphabetical */ 
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
	case 'v':
	    verbose = 1;
	    break;
	case 's':
	    (void) sleep((unsigned int) atoi(optarg));
	    break;
	case 'w':
	    width = atoi(optarg);
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
	    printf("Width, height and bitdepth are mandatory when reading from file\n");
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

	get_framebufferdata(device, &fb_varinfo, verbose);
	
	if (UNDEFINED == bitdepth)
	    bitdepth = (int) fb_varinfo.bits_per_pixel;
	
	if (UNDEFINED == width)
	    width = (int) fb_varinfo.xres;
	
	if (UNDEFINED == height)
	    height = (int) fb_varinfo.yres;

	printf("Resolution: %ix%i depth %i\n", width, height, bitdepth);

	strncpy(infile, device, MAX_LEN - 1);
    }
    
    buf_size = width * height * (((unsigned int) bitdepth + 7) >> 3);

    buf_p = malloc(buf_size);
    
    if(buf_p == NULL)
	fatal_error("Not enough memory");

    memset(buf_p, 0, buf_size);

    read_framebuffer(infile, buf_size, buf_p);

    if (UNDEFINED != old_vt)
	(void) change_to_vt((unsigned short int) old_vt);

    convert_and_write(buf_p, outfile, width, height, bitdepth, interlace);
   
    (void) free(buf_p);

    return 0;
}

