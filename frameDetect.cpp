#include "frameDetect.h"
#include "comicFrames.h"
#include <iostream>

using namespace std;

/*
 * This uses -1 as a flag for an invalid answer, which is 2^32-1 in an uint. Since there aren't any comics 
 * that have that many pixels, let alone labels, this won't be a problem.
 */

/*
 * Initializes a new FrameDetect by binarizing the image, determining bcolour, and 
 * adding white borders.
 */
FrameDetect::FrameDetect(AbstractImage<pixel>& page, uint pageNum) : rimg(page) {
	bimg = rimg.binarize(228);

	w = page.width();
	h = page.height();

	bcolour = determineBackground(bimg);
	ccolour = 255 - bcolour;
	FrameDetect::page = pageNum;

	addWhiteBorders();
}

FrameDetect::~FrameDetect() {}

//Public wrapper
ComicFrames FrameDetect::process() {
	return process(1, 1, w, h-1);
}

/*
 * Follows a contour (a line) and labels it differently
 */
void FrameDetect::contourTracking(LabelData& labelData, uint x, uint y, uint initialPos, uint lbl) {

	uint d = initialPos;
	Point s(x, y);
	
	bool atStart = false; //gotten back to initial position
	bool tFollowsS = false; //ahead of initial position (circled around)
	
	Point t = tracer(labelData, x, y, d, lbl);
	if (t.x < 0) { //isolated point
		return;
	}
	
	labelData(t.x, t.y) = lbl;
	x = t.x;
	y = t.y;
	
	while(!atStart || !tFollowsS) {
		d = (d + 6) % 8; //previous contour is d+4, next initial position is d+2
		
		Point f = tracer(labelData, x, y, d, lbl);
		/*cout << "f.x: " << f.x << 
			" f.y: " << f.y << 
			" x, y: " << x << "," << y <<
			" s.x: " << s.x << 
			" s.y: " << s.y << endl; */
		
		if (f.x < 0) { //image boundary
			return;
		}
		
		if (f == s) {
			atStart = true;
		} else if (f == t) {
			tFollowsS = atStart ? true : false;
		} else {
			atStart = tFollowsS = false;
		}
		
		labelData(f.x, f.y) = lbl;

		x = f.x;
		y = f.y;
	}
}

/*
 * Traces the outside of the shape
 */
FrameDetect::Point FrameDetect::tracer(LabelData& labelData, uint x, uint y, uint pos, uint lbl) {
	for (uint i = 7; i >= 0; --i) {
		uint tx = x;
		uint ty = y;
		nextPoint(tx, ty, pos);
		
		if (tx > 0 && ty > 0 && tx < w && ty < h) {
			uint l = labelData(tx,ty);
			
			if (bimg(tx,ty) == ccolour && (l == 0 || l == lbl)) {
				return Point(tx,ty);
			}
			if (bimg(tx,ty) == bcolour) {
				labelData(tx,ty) = -1;
			}
		}
		
		pos = (pos + 1) % 8;
	}
	return Point(-1, -1);
}

/*
 * Finds a bounding box for each frame and adds it to a ComicFrames, which it then returns
 */
ComicFrames FrameDetect::frames(LabelData& labelData) const {
	vector<uint> x1(label);
	vector<uint> x2(label);
	vector<uint> y1(label);
	vector<uint> y2(label);
	
	//initialize tables
	for (uint lbl=0; lbl<label; lbl++) {
		x1[lbl] = w;
		x2[lbl] = y2[lbl] = -1;
		y1[lbl] = h;
	}
	
	//find bounding boxes for labels
	for (uint y = 0; y < h; ++y) {
		for (uint x = 0; x < w; ++x) {
			
			uint lbl = labelData(x, y);
			
			if (lbl > 0) {
				if (x < x1[lbl]) { x1[lbl] = x; }
				if (x > x2[lbl]) { x2[lbl] = x; }
				if (y < y1[lbl]) { y1[lbl] = y; }
				if (y > y2[lbl]) { y2[lbl] = y; }
			}
		}
	}
	
	ComicFrames comicFrames(page, w, h);
	
	for (uint lbl = 0; lbl < label; ++lbl) {
		uint FW = x2[lbl]-x1[lbl];
		uint FH = y2[lbl]-y1[lbl];
		
		if ((FW >= w/6) && (FH >= h/8)) {
			comicFrames.append(comicFrame(x1[lbl], y1[lbl], w, h, lbl));
		}
	}
	
	//No frames, use image
	if (comicFrames.count() == 0) {
		comicFrames.append(comicFrame(0, 0, w, h, 0));
	}
	return comicFrames;
}

/*
 * The backbone of the algorithm, goes through each pixel and 
 * assigns a label depending on which frame it thinks it belongs to.
 */
ComicFrames FrameDetect::process(uint px, uint py, uint pw, uint ph) {
	LabelData labelData(w, h); //allocates more than required
	labelData.fill(0);
	
	label = 1;
	for(uint y = py; y < py + ph; ++y) {
		for (uint x = px; x < px + pw; ++x) {
			uint p = labelData(x,y);
			
			if (bimg(x,y) == ccolour) { //found contour pixel
				if (p == 0 && bimg(x,y-1) == bcolour) {
					//step 1
					p = label;
					contourTracking(labelData, x, y, 7, label++);
					
				} else if (labelData(x,y+1) == 0 && bimg(x,y+1) == bcolour) { //pixel below is unmarked
					//step 2
					if (p == 0) {
						p = labelData(x-1, y); //copy label
					}
					contourTracking(labelData, x, y, 3, p);
					
				} else if (p == 0) {
					p = labelData(x-1,y);
				}
			}
		}
	}
	
	return frames(labelData);
}

/*
 * Converts the boundary of the page into a bounding box of bcolour
 * so it has a start point & we can return the entire image as a frame
 * for the worst case of not finding any frames on a page.
 */
void FrameDetect::addWhiteBorders() {
	for(uint x = 0; x < w; ++x) {
		bimg(x,0) = bimg(x,h-1) = bcolour;
	}
	
	for(uint y = 1; y < h-1; ++y) {
		bimg(0,y) = bimg(w-1,y) = bcolour;
	}
}

/*
 * Background colour == the most common colour, which is important
 * only because it is /not/ the contour colour.
 */
byte FrameDetect::determineBackground(AbstractImage<byte>& img) {
	uint stripWidth = w/100; //small optimization, no real need to scan entire image
	
	uint black = 0;
	uint white = 0;
	
	for(uint y = 0; y < h; ++y) {
		for(uint x = 0; x < stripWidth; ++x) {
			if(img(x,y) == 0) {
				++black;
			} else {
				++white;
			}
			
			if(img(w-x-1, y) == 0) {
				++black;
			} else {
				++white;
			}
		}
	}
	
	return black > white ? 255: 0;
}
