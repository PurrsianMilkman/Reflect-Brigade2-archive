#define MAXTRACERS		16			// 
#define TRACERJOBSIZE	4			// amount of lines concidered one job

#define MINUV			(-100)		// smallest UV allowed (for negative UV space)
#define MAXDEPTH		2			// maximum trace depth
#define TOPBVHSIZE		1024		// nodes

#define MAXLIGHTS		15000
#define SENTINEL		0x76543210	// BVH entry point sentinel
#define MAXPRESPLIT		15000		// from brigade 1?
#define LIGHTTABLESIZE	512
#define IS_SHADOW		4			// rename me?
#define SAMPLEDIMS		2			// sampler dimensions: 0 = light, 1 = diffuse bounce

#define RAYLENGTH		1000000.0f	// maximum length of a ray
#define BACKSIDECULLING				// disallow two-sided triangles

// #define SUNRAY						// enables a extra shadow ray always fired in a given direction

#define GAMMACORRECTION
// #define MAKETRANSPARENTSURFACESDOUBLESIDED

 #define BILINEAR_FILTERING

// #define BLOOM					// basic CPU bloom

#define EPSILON			0.00001f

// #define COMPRESSNORMALS			// sometimes its faster, but less accuracy

#define BRIGADEDEBUG				// enableds some prinfs for debugging/development

#define CSIZE 7						// 1<<CSIZE = chunksize for tracerheap

// SKY SETTINGS
#define SKY_OPTIMIZE				// assumes the sky is 2k^2
// set sky format
// #define SKY_PROBEMAP				// probemap
#define SKY_UTMAP					// unreal tournament format

// CONTENT SETTINGS
#define MESHLOADERVERSION 32
