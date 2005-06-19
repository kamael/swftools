/* swfbbox.c
   Tool for playing around with SWF bounding boxes.

   Part of the swftools package.
   
   Copyright (c) 2003 Matthias Kramm <kramm@quiss.org>
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "../config.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include "../lib/rfxswf.h"
#include "../lib/args.h"
#include "../lib/log.h"

static char * filename = 0;
static char * outfilename = "output.swf";
static int optimize = 0;
static int swifty = 0;
static int verbose = 0;
static int showbbox = 0;
static int showorigbbox = 1;
static int expand = 1;

static struct options_t options[] = {
{"h", "help"},
{"b", "bbox"},
{"B", "newbbox"},
{"e", "expand"},
{"O", "optimize"},
{"S", "swifty"},
{"o", "output"},
{"v", "verbose"},
{"V", "version"},
{0,0}
};

int args_callback_option(char*name,char*val)
{
    if(!strcmp(name, "V")) {
        printf("swfbbox - part of %s %s\n", PACKAGE, VERSION);
        exit(0);
    } 
    else if(!strcmp(name, "b")) {
	showorigbbox = 1;
	if(showbbox == 1) showbbox = 0;
	return 0;
    } 
    else if(!strcmp(name, "B")) {
	showbbox = 2;
	return 0;
    } 
    else if(!strcmp(name, "O")) {
	optimize = 1;
	if(showbbox == 1) showbbox = 0;
	return 0;
    } 
    else if(!strcmp(name, "S")) {
	swifty = 1;
	if(showbbox == 1) showbbox = 0;
	return 0;
    } 
    else if(!strcmp(name, "v")) {
	verbose ++;
	return 0;
    } 
    else if(!strcmp(name, "q")) {
	verbose --;
	return 0;
    } 
    else if(!strcmp(name, "e")) {
	expand = 1;
	return 0;
    } 
    else if(!strcmp(name, "o")) {
	outfilename = val;
	return 1;
    } 
    else {
        printf("Unknown option: -%s\n", name);
	exit(1);
    }

    return 0;
}
int args_callback_longoption(char*name,char*val)
{
    return args_long2shortoption(options, name, val);
}
void args_callback_usage(char *name)
{
    printf("\n");
    printf("Usage: %s [-OS] file.swf\n", name);
    printf("\n");
    printf("-h , --help                    Print help and exit\n");
    printf("-b , --bbox                    Show movie bounding box (default)\n");
    printf("-B , --newbbox                 Show recalculated (optimized/expanded) bounding box\n");
    printf("-e , --expand                  Write out a new file using the recalculated bounding box\n");
    printf("-O , --optimize                Recalculate bounding boxes\n");
    printf("-S , --swifty                  Print out transformed bounding boxes\n");
    printf("-o , --output <filename>       Set output filename to <filename> (for -O)\n");
    printf("-v , --verbose                 Be more verbose\n");
    printf("-V , --version                 Print program version and exit\n");
    printf("\n");
}
int args_callback_command(char*name,char*val)
{
    if(filename) {
        fprintf(stderr, "Only one file allowed. You supplied at least two. (%s and %s)\n",
                 filename, name);
    }
    filename = name;
    return 0;
}

#define swf_ResetReadBits(tag)   if (tag->readBit)  { tag->pos++; tag->readBit = 0; }

void swf_Shape2Optimize(SHAPE2*shape)
{
    if(!shape->bbox)
	shape->bbox = malloc(sizeof(SRECT));
    *(shape->bbox) = swf_GetShapeBoundingBox(shape);
}

/*
   {char {x1 y1 x2 y2 x3 y3 x4 y4]]
*/

SRECT bboxes[65536];
U16 depth2id[65536];
char*depth2name[65536];

int hasid(TAG*tag)
{
    if(tag->id == ST_PLACEOBJECT)
	return 1;
    if(tag->id == ST_PLACEOBJECT2 && (tag->data[0] & 2))
	return 1;
    return 0;
}

int hasname(TAG*tag)
{
    if(tag->id == ST_PLACEOBJECT)
	return 0;
    if(tag->id == ST_PLACEOBJECT2 && (tag->data[0] & 0x20))
	return 1;
    return 0;
}

char* getname(TAG*tag)
{
    if(tag->id == ST_PLACEOBJECT)
	return 0;
    if(tag->id == ST_PLACEOBJECT2 && (tag->data[0] & 0x20)) {
	SWFPLACEOBJECT o;
	tag->pos = 0;tag->readBit = 0;
	swf_GetPlaceObject(tag, &o);
	return o.name;
    }
    return 0;
}

MATRIX getmatrix(TAG*tag)
{
    SWFPLACEOBJECT o;
    tag->pos = 0;tag->readBit = 0;
    swf_GetPlaceObject(tag, &o);
    return o.matrix;
}


static int fontnum = -1;
static SWFFONT**fonts;
static SWF*c_swf;
static void fontcallback1(void*self, U16 id,U8 * name)
{ fontnum++;
}
static void fontcallback2(void*self, U16 id,U8 * name)
{ 
    fonts[fontnum] = 0;
    swf_FontExtract(c_swf,id,&fonts[fontnum]);
    if(verbose) {
	if(fonts[fontnum]) printf("Extracting font %d (%s)\n", id, name);
	else		   printf("Extracting font %d (%s) failed\n", id, name);
	fflush(stdout);
    }
    fontnum++;
}
typedef struct _textbounds
{
    SRECT r;
    MATRIX m; // character transform matrix
} textbounds_t;

static void textcallback(void*self, int*chars, int*xpos, int nr, int fontid, int fontsize, 
		    int xstart, int ystart, RGBA* color)
{
    textbounds_t * bounds = (textbounds_t*)self;
    SWFFONT*font = 0;
    int t;
    for(t=0;t<fontnum;t++) {
	if(fonts[t]->id == fontid) {
	    font = fonts[t];
	    break;
	}
    }
    if(!font) {
	fprintf(stderr, "Font %d unknown\n", fontid);
	exit(1);
    }
    if(!font->layout) {
	/* This is an expensive operation- but what should we do, we
	   need the glyph's bounding boxes */
	swf_FontCreateLayout(font);
    }

    if(verbose)
	printf("%d chars, font %d, size %d, at (%d,%d)\n", nr, fontid, fontsize, xstart, ystart);

    for(t=0;t<nr;t++) {
	/* not tested yet- the matrix/fontsize calculation is probably all wrong */
	int x = xstart + xpos[t];
	int y = ystart;
	int ch;
	SRECT newglyphbbox, glyphbbox = font->layout->bounds[chars[t]];
	MATRIX m = bounds->m;
	SPOINT p;

	if(chars[t] < font->numchars && font->glyph2ascii) {
	    ch = font->glyph2ascii[chars[t]];
	}

	p.x = x; p.y = y;
	p = swf_TurnPoint(p, &m);

	m.sx = (m.sx * fontsize) / 1024;
	m.sy = (m.sy * fontsize) / 1024;
	m.r0 = (m.r0 * fontsize) / 1024;
	m.r1 = (m.r1 * fontsize) / 1024;

	m.tx += p.x;
	m.ty += p.y;
	newglyphbbox = swf_TurnRect(glyphbbox, &m);

	if(ch<32) ch='?';
	    
	swf_ExpandRect2(&(bounds->r), &newglyphbbox);
	if(verbose >= 2) {
	    printf("%5d %c, %d %d %d %d (%d %d %d %d) -> %d %d %d %d\n", 
		xpos[t], ch, 
		glyphbbox.xmin, glyphbbox.ymin, glyphbbox.xmax, glyphbbox.ymax,
		newglyphbbox.xmin, newglyphbbox.ymin, newglyphbbox.xmax, newglyphbbox.ymax,
		bounds->r.xmin, bounds->r.ymin, bounds->r.xmax, bounds->r.ymax);
	}

    }
}

static void swf_OptimizeBoundingBoxes(SWF*swf)
{
    TAG* tag = swf->firstTag;
    
    while (tag) {
	if (tag->id == ST_DEFINESHAPE ||
	    tag->id == ST_DEFINESHAPE2 ||
	    tag->id == ST_DEFINESHAPE3) {
	    SHAPE2 s;
	    if(verbose) printf("%s\n", swf_TagGetName(tag));
	    swf_ParseDefineShape(tag, &s);
	    swf_Shape2Optimize(&s);
	    tag->len = 2;
	    tag->pos = 0;
	    swf_SetShape2(tag, &s);
	}
	if (tag->id == ST_DEFINETEXT || tag->id == ST_DEFINETEXT2) {
	    SRECT oldbox;
	    int matrix_offset;
	    int len;
	    U8*data;
	    textbounds_t bounds;
	    if(verbose) printf("%s\n", swf_TagGetName(tag));
	    if(fontnum < 0) {
		if(verbose) printf("Extracting fonts...\n");
		c_swf = swf;
		fontnum = 0;
		swf_FontEnumerate(swf,&fontcallback1,0);
		fonts = (SWFFONT**)malloc(fontnum*sizeof(SWFFONT*));
		memset(fonts, 0, fontnum*sizeof(SWFFONT*));
		fontnum = 0;
		swf_FontEnumerate(swf,&fontcallback2,0);
	    }

	    memset(&bounds, 0, sizeof(bounds));

	    swf_SetTagPos(tag, 0);
	    swf_GetU16(tag);
	    swf_GetRect(tag,&oldbox);
	    swf_ResetReadBits(tag);
	    matrix_offset = tag->pos;
  	    swf_GetMatrix(tag,&bounds.m);
	    swf_ParseDefineText(tag, textcallback, &bounds);
	    if(verbose) {
		printf("\n");
		swf_DumpMatrix(stdout, &bounds.m);
	    	printf("old: %d %d %d %d\n", oldbox.xmin, oldbox.ymin, oldbox.xmax, oldbox.ymax);
	    	printf("new: %d %d %d %d\n", bounds.r.xmin, bounds.r.ymin, bounds.r.xmax, bounds.r.ymax);
	    }
	    
	    /* now comes the tricky part: 
	       we have to fiddle the data back in 
	       thank heavens that the bbox is follow by a matrix
	       struct, which always starts on a byte boundary.
	     */
	    len = tag->len - matrix_offset;
	    data = malloc(len);
	    memcpy(data, &tag->data[matrix_offset], len);
	    tag->writeBit = 0;
	    tag->len = 2;
	    swf_SetRect(tag, &bounds.r);
	    swf_SetBlock(tag, data, len);
	    free(data);
	    tag->pos = tag->readBit = 0;
	}
	tag = tag->next;
    }
}

static void showSwiftyOutput(SWF*swf) 
{
    TAG*tag = swf->firstTag;
    int frame=0;
    printf("{\n\t{frame %d}\n", frame++);

    while (tag) {
	if (tag->id == ST_SHOWFRAME) {
	    printf("}\n{\n\t{frame %d}\n", frame++);
	}
	if (tag->id == ST_PLACEOBJECT || tag->id == ST_PLACEOBJECT2) {
	    if(hasid(tag)) {
		depth2id[swf_GetDepth(tag)] = swf_GetPlaceID(tag);
	    }
	    if(hasname(tag)) {
		depth2name[swf_GetDepth(tag)] = getname(tag);
	    }
	}
	if (tag->id == ST_PLACEOBJECT || tag->id == ST_PLACEOBJECT2) {
	    MATRIX m = getmatrix(tag);
	    U16 id = depth2id[swf_GetDepth(tag)];
	    char*name = depth2name[swf_GetDepth(tag)];
	    char buf[40];
	    SRECT bbox = bboxes[id];
	    SPOINT p1,p2,p3,p4;
	    p1.x = bbox.xmin; p1.y = bbox.ymin;
	    p2.x = bbox.xmax; p2.y = bbox.ymin;
	    p3.x = bbox.xmin; p3.y = bbox.ymax;
	    p4.x = bbox.xmax; p4.y = bbox.ymax;
	    p1 = swf_TurnPoint(p1, &m);
	    p2 = swf_TurnPoint(p2, &m);
	    p3 = swf_TurnPoint(p3, &m);
	    p4 = swf_TurnPoint(p4, &m);
	    if(!name) {
		sprintf(buf, "ID%d", id);name = buf;
	    }
	    //printf("\t#%.4f %.4f %.4f %.4f | %.4f %.4f\n", m.sx/65536.0, m.r1/65536.0, m.r0/65536.0, m.sy/65536.0, m.tx/20.0, m.ty/20.0);
	    printf("\t{%s {%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f}}\n", name, 
		    p1.x/20.0, p1.y/20.0, p2.x/20.0, p2.y/20.0,
		    p3.x/20.0, p3.y/20.0, p4.x/20.0, p4.y/20.0);
	}
	tag = tag->next;
    }
    printf("}\n");
}
static SRECT getMovieClipBBox(TAG*tag) 
{
    //TAG*tag = swf->firstTag;
    int frame=0;
    SRECT movieSize;
    U16 depth2id[65536];
    memset(depth2id, 0, sizeof(depth2id));

    memset(&movieSize,0,sizeof(SRECT));

    while (tag->id != ST_END) {
	if (tag->id == ST_PLACEOBJECT || tag->id == ST_PLACEOBJECT2) {
	    if(hasid(tag)) {
		depth2id[swf_GetDepth(tag)] = swf_GetPlaceID(tag);
	    }
	}
	if (tag->id == ST_PLACEOBJECT || tag->id == ST_PLACEOBJECT2) {
	    MATRIX m = getmatrix(tag);
	    U16 id = depth2id[swf_GetDepth(tag)];
	    SRECT bbox = bboxes[id];
	    
	    SRECT tbbox = swf_TurnRect(bbox, &m);
	    swf_ExpandRect2(&movieSize, &tbbox);
	}
	tag = tag->next;
    }
    return movieSize;
}

static SRECT getSWFBBox(SWF*swf)
{
    SRECT movieSize = getMovieClipBBox(swf->firstTag);
    
    return movieSize;
}

int main (int argc,char ** argv)
{ 
    TAG*tag;
    SWF swf;
    int fi;
    SRECT oldMovieSize;
    SRECT newMovieSize;
    memset(bboxes, 0, sizeof(bboxes));
    memset(depth2name, 0, sizeof(depth2name));

    processargs(argc, argv);
    initLog(0,0,0,0,0,verbose?LOGLEVEL_DEBUG:LOGLEVEL_WARNING);

    if(!filename) {
        fprintf(stderr, "You must supply a filename.\n");
        return 1;
    }

    fi = open(filename,O_RDONLY|O_BINARY);

    if (fi<0)
    { 
        perror("Couldn't open file: ");
        exit(1);
    }
    if FAILED(swf_ReadSWF(fi,&swf))
    { 
        fprintf(stderr, "%s is not a valid SWF file or contains errors.\n",filename);
        close(fi);
        exit(1);
    }
    close(fi);

    swf_OptimizeTagOrder(&swf);
    swf_FoldAll(&swf);

    /* Optimize bounding boxes in case -O flag was set */
    if(optimize) {
	swf_OptimizeBoundingBoxes(&swf);
    }
    
    /* Create an ID to Bounding Box table */
    tag = swf.firstTag;
    while (tag) {
	if(swf_isDefiningTag(tag)) {
	    int id = swf_GetDefineID(tag);
	    if(tag->id != ST_DEFINESPRITE) {
		bboxes[id] = swf_GetDefineBBox(tag);
	    } else {
		swf_UnFoldSprite(tag);
		bboxes[id] = getMovieClipBBox(tag);
		swf_FoldSprite(tag);
		if(verbose) {
		    printf("sprite %d is %.2fx%.2f\n", id, 
			    (bboxes[id].xmax - bboxes[id].xmin)/20.0,
			    (bboxes[id].ymax - bboxes[id].ymin)/20.0);
		}
	    }
	}
	tag = tag->next;
    }
    
    /* Create an ID->Bounding Box table for all bounding boxes */
    if(swifty) {
	showSwiftyOutput(&swf);
    }

    oldMovieSize = swf.movieSize;
    newMovieSize = getSWFBBox(&swf);

    if(optimize || expand) {

	if(expand)
	    swf.movieSize = newMovieSize;

	fi = open(outfilename, O_BINARY | O_RDWR | O_CREAT | O_TRUNC, 0666);
	if(swf_WriteSWF(fi, &swf) < 0) {
	    fprintf(stderr, "Error writing file %s", outfilename);
	    close(fi);
	    exit(1);
	}
	close(fi);
    }
    
    if(showbbox) {
	if(verbose>=0)
	    printf("Real Movie Size: ");
	printf("%.2f x %.2f :%.2f :%.2f\n", 
		(newMovieSize.xmax-newMovieSize.xmin)/20.0,
		(newMovieSize.ymax-newMovieSize.ymin)/20.0,
		(newMovieSize.xmin)/20.0,
		(newMovieSize.ymin)/20.0
		);
    }
    if(showorigbbox) {
	if(verbose>=0)
	    printf("Original Movie Size: ");
	printf("%.2f x %.2f :%.2f :%.2f\n", 
		(oldMovieSize.xmax-oldMovieSize.xmin)/20.0,
		(oldMovieSize.ymax-oldMovieSize.ymin)/20.0,
		(oldMovieSize.xmin)/20.0,
		(oldMovieSize.ymin)/20.0
		);
    }

    swf_FreeTags(&swf);
    return 0;
}
