// gewellt-eight
// (c) 2018 John Howard, orthopteroid@gmail.com
// MIT license

#define GLX_GLXEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/normal.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <time.h>
#include <math.h>
#include <vector>
#include <memory>

#include <ft2build.h>
#include <iostream>
#include <random>
#include <functional>
#include FT_FREETYPE_H

const int numTris = 8; // 8 tris, hence the name...
const int popSize = 200;

// ending criteria
const int maxIter = 200;
const float coverageTarget = .05;

const char* szFont = "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf";
const uint8_t* u8zGlyphs = (uint8_t*)u8"!-./0123456789?ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const FT_Encoding glyphCharmap = FT_ENCODING_UNICODE;

////////////////////////////////////////////

FT_Error ftError;
#define FT_CALL(x) do { ftError = (x); assert(!ftError); } while(false)

FT_Library ftLibrary;
FT_Face ftFace;
FT_Glyph_Metrics glyphMetrics;

uint8_t glyphWidth = 1;
uint8_t glyphRows = 1;
GLuint glyphHandle = 0;

// when using -Wall unused return values make for chatty logs...
#define IGNORE_RESULT(fn) if (fn)

std::random_device randSeed;
std::default_random_engine randValue(randSeed());

const int resolution = 64; // a power of 2 <= 256, but 64 might be a more reasonable number
const int texDensity = resolution, texSize = texDensity * texDensity;
const int viewDensity = texDensity -1, viewSize = viewDensity * viewDensity;

const int numPoints = numTris * 3;
const int numValues = numPoints * 2;

int pickPixel();

struct Population; // fwd ref

struct Triangle
{
    GLshort t[ 2 * 3 ]; // 3 sets of 2 cords

    inline Triangle& copy(Triangle& src)
    {
        memcpy( t, src.t, sizeof(Triangle) );
        return *this;
    }
    inline void adjust(uint16_t d)
    {
        auto r = uint32_t(randValue()); // call reduction
        auto r8 = r >> 8;
        auto r16 = r >> 16;

        auto d2p1 = d * 2 + 1;
        auto ic = r % 3;
        t[ic*2+0] = std::min<GLshort>( std::max<GLshort>( t[ic*2+0] + r8 % d2p1 - d, 0 ), glyphWidth );
        t[ic*2+1] = std::min<GLshort>( std::max<GLshort>( t[ic*2+1] + r16 % d2p1 - d, 0 ), glyphRows );
    }
    inline void shift(uint16_t d)
    {
        auto r = uint32_t(randValue()); // call reduction
        auto r8 = r >> 8;

        auto d2p1 = d * 2 + 1;
        auto qx = r % d2p1 - d;
        auto qy = r8 % d2p1 - d;

        for( uint16_t i=0; i<3; i++)
        {
            t[i * 2 + 0] = std::min<GLshort>( std::max<GLshort>( t[i * 2 + 0] + qx, 0 ), glyphWidth );
            t[i * 2 + 1] = std::min<GLshort>( std::max<GLshort>( t[i * 2 + 1] + qy, 0 ), glyphRows );
        }
    }
    inline void random(int featureSize)
    {
        uint8_t feaMin = 6, feaMax = feaMin + featureSize;

        auto pp = pickPixel();
        auto x = pp % texDensity;
        auto y = texDensity - pp / texDensity; // top down

        uint32_t r;
        uint8_t rArr[6];
        r = uint32_t(randValue()); // call reduction
        rArr[0] = uint8_t(r);
        rArr[1] = uint8_t(r)>>8;
        rArr[2] = uint8_t(r>>16);
        r = uint32_t(randValue()); // call reduction
        rArr[3] = uint8_t(r);
        rArr[4] = uint8_t(r>>8);
        rArr[5] = uint8_t(r>>16);

        int ri = 0;
        for( uint16_t i=0; i<3; i++)
        {
            t[i * 2 + 0] = x + GLshort(rArr[ri++] % ((feaMax - feaMin) * 2) - ((feaMax - feaMin) / 2));
            t[i * 2 + 1] = y + GLshort(rArr[ri++] % ((feaMax - feaMin) * 2) - ((feaMax - feaMin) / 2));
        }

    }
};
struct TriangleSet
{
    Triangle ts[ numTris ];

    inline TriangleSet& copy(TriangleSet& src)
    {
        memcpy( ts, src.ts, sizeof(TriangleSet) );
        return *this;
    }
    inline void merge(TriangleSet &src, uint8_t tsrc) { ts[tsrc].copy( src.ts[tsrc] ); }
    inline void adjust(uint16_t d) { ts[randValue() % numTris].adjust( d ); }
    inline void shift(uint16_t d) { ts[randValue() % numTris].shift( d ); }
    inline void random(int featureSize) { ts[randValue() % numTris].random(featureSize); }
    inline void randomize(int featureSize) { for(uint16_t i=0;i<numTris; i++) ts[i].random(featureSize); }
    inline void join()
    {
        auto r = uint32_t(randValue()); // call reduction
        auto t1 = r % numTris;
        auto t2 = (r >> 8) % numTris;
        auto v1 = (r >> 16) % 3; // [0,2]
        auto v2 = (r >> 24) % 3;

        ts[t2].t[v2*2+0] = ts[t1].t[v1*2+0]; // dup vert to join tris
        ts[t2].t[v2*2+1] = ts[t1].t[v1*2+1];
    }
    void crossover(Population& src, uint ms1, uint ms2);
};
struct Population {
    TriangleSet data[ popSize ];
    uint value[ popSize ];

    inline void scan(uint16_t &iBest, uint &vBest, uint &vSum)
    {
        vBest = 0; iBest = 0;
        for( uint16_t i = 0; i < popSize; i++ )
        {
            if( value[i] > vBest ) vBest = value[iBest = i];
        }

        // clear best and clones and make sum
        vSum = 0;
        for(uint16_t i=0; i<popSize; i++)
        {
            if( value[i] == vBest ) value[i] = 0;
            vSum += value[ i ];
        }
        vSum++; // +1 will put % operator in range of [0,sum]
    }
    inline void sum(uint16_t& m1, uint16_t& m2, uint ms1, uint ms2)
    {
        // scan and switch
        m1 = m2 = 0;
        for(uint16_t i=0; i<popSize; i++)
        {
            if(ms1 > 0) { ms1 -= value[ i ]; m1 = i; }
            if(ms2 > 0) { ms2 -= value[ i ]; m2 = i; }
        }
    };
};
Population* pPop = 0;

// for code cleanliness, this is better in TriangleSet than in Population
void TriangleSet::crossover(Population& src, uint ms1, uint ms2)
{
    uint16_t m1, m2;
    src.sum(m1,m2,ms1,ms2);
    if(randValue() & 1) std::swap( m1, m2 );

    copy( src.data[m1] ).merge( src.data[m2], uint8_t( randValue() % numTris ));
};

//////////////////////////////////
// objective function state

// samplers
static GLubyte bufR[texSize];
static GLubyte bufG[texSize];
static GLubyte bufB[texSize];

static GLubyte bufR_picker[texSize];
static uint sumPicker = 0;
static int totalRed = 0;

// statistics
uint blue; // background
uint red; // glyph
uint green; // triangle output
uint green_over_blue; // unnecessary triangle output on background
uint green_over_green; // unnecessary multi-layered triangle output
uint smallFeature;

///////////////////////////////////
// state vars

static uint8_t* u8Glyph = (uint8_t*)u8zGlyphs; // pointer to next utf8 char
static char sprintfBuffer[80];
static char utf8Buffer[4 +1]; // +1 for \0, static is not threadsafe

uint currentMember = popSize-1;
uint iteration = 0;
FILE *pResultFile = 0;

//////////////////////

// inc ptr according to length of current utf8 char
// https://en.wikipedia.org/wiki/UTF-8#Description
inline int u8_charlength(uint8_t* ptr)
{
    if(*ptr == '\0') return 0;
    int expect = *ptr <= 127 ? 1 : (*ptr <= 223 ? 2 : (*ptr <= 239 ? 3 : (*ptr <= 277 ? 0 : 4)));
    if(expect == 1) return 1; // fastpath

    // check for misplaced continuation bytes
    if(expect == 0) return 0; // misplaced continuation byte
    if(expect >= 2 && (ptr[1] & 192) != 128) return 1; // not a continuation
    if(expect >= 3 && (ptr[2] & 192) != 128) return 2; // not a continuation
    if(expect == 4 && (ptr[3] & 192) != 128) return 3; // not a continuation
    return expect;
}

char* u8_composeString(uint8_t *ptr)
{
    int i=0;
    while( i < u8_charlength(ptr) ) { utf8Buffer[i] = ptr[i]; i++; }
    utf8Buffer[i] = 0;
    return utf8Buffer;
}

FT_ULong u8_composeLong(uint8_t* ptr)
{
    FT_ULong result = 0;
    auto L = u8_charlength(ptr);
    for(int i=0; i < L; i++ ) result = (result << 8) + ptr[i];
    return result;
}

//////////////////////

int pickPixel()
{
    if(sumPicker == 0)
        return randValue() % viewSize;

    auto r = randValue() % sumPicker +1; // +1 for cover (1,N)
    int i = 0;
    while( r > 0 && i++<texSize) { if(bufR_picker[i] > 0) r--; } // review: ++ in conditional!
    return i;
}

uint measure(std::function<uint(uint)> fn)
{
    uint m = 0;
    for(uint i=0;i<texSize;i++) m += fn(i);
    return m;
}

void texture(bool dump = false)
{
    auto G = u8_composeLong( u8Glyph );
    auto glyph_index = FT_Get_Char_Index( ftFace, G );

    FT_CALL( FT_Load_Glyph( ftFace, glyph_index, FT_LOAD_DEFAULT ) );
    FT_CALL( FT_Render_Glyph( ftFace->glyph, FT_RENDER_MODE_NORMAL ) );

    glyphWidth = uint8_t(ftFace->glyph->bitmap.width);
    glyphRows = uint8_t(ftFace->glyph->bitmap.rows);
    glyphMetrics = ftFace->glyph->metrics;

    // https://www.freetype.org/freetype2/docs/tutorial/step2.html
    // convert metrics to pixels
    glyphMetrics.width /= 64;
    glyphMetrics.height /= 64;
    glyphMetrics.horiBearingX /= 64;
    glyphMetrics.horiBearingY /= 64;
    glyphMetrics.horiAdvance /= 64;
    glyphMetrics.vertBearingX /= 64;
    glyphMetrics.vertBearingY /= 64;
    glyphMetrics.vertAdvance /= 64;

    struct Texel { GLubyte l, a; } texPixels[ texSize ]; // luminance & alpha

    memset(texPixels,0,sizeof(texPixels));
    for(uint j = 0; j < ftFace->glyph->bitmap.rows; j++)
        for(uint i = 0; i < ftFace->glyph->bitmap.width; i++)
            texPixels[ i + j * texDensity ] = { 255, ftFace->glyph->bitmap.buffer[ i + j * ftFace->glyph->bitmap.width ] };

    glBindTexture( GL_TEXTURE_2D, glyphHandle );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, texDensity, texDensity, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, texPixels );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // fix weird edge artifacts
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture( GL_TEXTURE_2D, 0 );

    if(dump)
    {
        for( uint j = 0; j < ftFace->glyph->bitmap.rows; j++ )
        {
            for( uint i = 0; i < ftFace->glyph->bitmap.width; i++ ) printf( "%02X", texPixels[ i + j * texDensity ].a );
            putchar( '\n' );
        }
    }
}

void dumpGewelltGlyph(uint curPop)
{
    fprintf( pResultFile, ",{ '%s',    ", u8_composeString( u8Glyph ));
    auto pMetrics = (FT_Pos*)&glyphMetrics;
    for(uint m = 0; m < 8; m++ ) fprintf( pResultFile, "%d,", int(pMetrics[m]) );
    fprintf( pResultFile, "    " );
    auto pValues = (GLshort *) &pPop[curPop].data[0];
    for( uint v = 0; v < numValues; v++ ) fprintf( pResultFile, "%d,", pValues[v] );
    fprintf( pResultFile, "}\n" );
    fflush(pResultFile);
}

//////////////////////////////////////

static bool saverequest = false;

void key(unsigned char c, int x, int y)
{
    uint curPop = iteration & 1;

    if(c=='r') currentMember = (popSize-1), iteration = 0; // reset
    if(c=='s') saverequest = true; // save

    if(c=='S')
    {
        snprintf(sprintfBuffer, sizeof(sprintfBuffer), "scrot \\%s.png -u", u8_composeString( u8Glyph ));
        //IGNORE_RESULT(system((const char*) sprintfBuffer));

        // and to convert mp4 screen-cap into a gif use:
        // ffmpeg -i digits-8tri.mp4 -pix_fmt rgb24 digits-8tri.gif
    }

    if(c=='n' || c=='S') // next
    {
        dumpGewelltGlyph( curPop );

        u8Glyph += u8_charlength( u8Glyph );
        if(*u8Glyph == '\0') // endof glyph string? quit!
            c = 27;
        else
        {
            currentMember = ( popSize - 1 );
            iteration = 0;

            randValue.seed(randSeed());
            texture();
        }
    }

    if(c == 27) // esc quits
    {
        glDeleteTextures( sizeof( glyphHandle ), &glyphHandle );
        FT_Done_Face( ftFace );
        FT_Done_FreeType( ftLibrary );
        delete[] pPop;
        fclose(pResultFile);
        exit( 0 );
    }

}

void display(void)
{
    uint curPop = iteration & 1;

    // background blue
    glClearColor(0,0,1,1);
    glClear(GL_COLOR_BUFFER_BIT);

    ////////////////////////////////
    // draw current trial

    // glyph in red
    glColor4f(1,0,0,1);
    glBindTexture( GL_TEXTURE_2D, glyphHandle );
    glBegin(GL_QUADS);
    glTexCoord2i(0,0); glVertex2i(0,0);
    glTexCoord2i(0,1); glVertex2i(0,viewDensity);
    glTexCoord2i(1,1); glVertex2i(viewDensity,viewDensity);
    glTexCoord2i(1,0); glVertex2i(viewDensity,0);
    glEnd();
    glBindTexture( GL_TEXTURE_2D, 0 );

    // on first glyph render we sample the blue reading, before we write green crap on it.
    if(currentMember==(popSize-1))
    {
        glFlush();
        ::glReadPixels( 0, 0, texDensity-1, texDensity-1, GL_BLUE, GL_UNSIGNED_BYTE, bufB );
        ::glReadPixels( 0, 0, texDensity-1, texDensity-1, GL_RED, GL_UNSIGNED_BYTE, bufR );

        blue = measure([&](uint i) { return bufB[i] > 0 ? 1 : 0; } );
        totalRed = measure([&](uint i) { return bufR[i] > 0 ? 1 : 0; } );
    }

    const float greenAlpha = .25;

    // tris in alpha green
    glColor4f(0,1,0,.25f);
    glEnableClientState( GL_VERTEX_ARRAY );
    glVertexPointer( 2, GL_SHORT, 0, &pPop[curPop].data[currentMember] ); // [currentMember] is current trial
    glDrawArrays(GL_TRIANGLES, 0, numPoints);
    glDisableClientState( GL_VERTEX_ARRAY );

    ////////////////////////////////
    // draw best

    glPushMatrix();
    glTranslatef(GLfloat(viewDensity),0,0);

    // glyph in red
    glColor4f(1,0,0,1);
    glBindTexture( GL_TEXTURE_2D, glyphHandle );
    glBegin(GL_QUADS);
    glTexCoord2i(0,0); glVertex2i(0,0);
    glTexCoord2i(0,1); glVertex2i(0,viewDensity);
    glTexCoord2i(1,1); glVertex2i(viewDensity,viewDensity);
    glTexCoord2i(1,0); glVertex2i(viewDensity,0);
    glEnd();
    glBindTexture( GL_TEXTURE_2D, 0 );

    // tris in green
    glColor3f(0,1,0);
    glEnableClientState( GL_VERTEX_ARRAY );
    glVertexPointer( 2, GL_SHORT, 0, &pPop[curPop].data[0] ); // [0] is best
    glDrawArrays(GL_TRIANGLES, 0, numPoints);
    glDisableClientState( GL_VERTEX_ARRAY );

    glPopMatrix();

    ////////////////////////////////
    // sample

    glFlush();
    ::glReadPixels( 0, 0, texDensity-1, texDensity-1, GL_RED, GL_UNSIGNED_BYTE, bufR );
    ::glReadPixels( 0, 0, texDensity-1, texDensity-1, GL_GREEN, GL_UNSIGNED_BYTE, bufG );

    red = measure([&](uint i) { return bufR[i] > 250 ? 1 : 0; } );
    green = measure([&](uint i) { return bufG[i] > 0 ? 1 : 0; } );
    green_over_green = measure([&](uint i) { return bufG[i] > uint(greenAlpha * 255.f) +5 ? 1 : 0; } );
    green_over_blue = measure([&](uint i) { return (bufG[i] & bufB[i]) ? 1 : 0; } );

    // update the feature positioning seed
    ::glReadPixels( 0, 0, texDensity-1, texDensity-1, GL_RED, GL_UNSIGNED_BYTE, bufR_picker );
    sumPicker = measure([&](uint i) { return bufR_picker[i] > 0 ? 1 : 0; } );

    // save after drawing of 'best'
    if(saverequest && currentMember == 0)
    {
        saverequest = false;

        key('S',0,0);
    }
}

void idle()
{
    uint curPop = iteration & 1;
    uint newPop = ~iteration & 1;

    // check for seed-phase
    if(iteration==0 && currentMember==(popSize-1))
    {
        for( uint16_t i = 0; i < popSize; i++ ) pPop[0].data[i].randomize( 8 );
        for( uint16_t i = 0; i < popSize; i++ ) pPop[1].data[i].randomize( 8 );
        for( uint16_t i = 0; i < popSize; i++ ) pPop[0].value[i] = 0;
        for( uint16_t i = 0; i < popSize; i++ ) pPop[1].value[i] = 0;
    }
    else
    {
        // quick check for tris that share cords - and are likely zero size
        const int smallSize = 3;
        smallFeature = 0;
        struct xy { GLshort x, y; };
        auto p = (xy*) &pPop[curPop].data[currentMember];
        for( uint16_t t = 0; t < numTris; t++ )
            smallFeature += (
                      abs(p[t].x-p[t+1].x) * abs(p[t].y-p[t+1].y) < smallSize ||
                      abs(p[t].x-p[t+2].x) * abs(p[t].y-p[t+2].y) < smallSize ||
                      abs(p[t+1].x-p[t+2].x) * abs(p[t+1].y-p[t+2].y) < smallSize
                      ) ? 1 : 0;

        // default objective function weights:
        // expose as much blue as possible while covering red and preventing triangle overlap
        uint a = 5, b = 3, c = 2, d = 1;

        if( iteration < maxIter * 1 / 10 )
        {
            // weight permutations allow annealing
            int batch = (iteration % 41) / 20; // [0,40] -> [0,2] in blocks of 20
            switch( batch )
            {
                default: break;
                case 1: std::swap(a,c); break; // perfer less triangle overlap to glyph coverage -> deflate
                case 2: std::swap(a,b); break; // prefer background coverage to glyph coverage -> inflate
            }
        }
        else
        {
            // finishing phase: small feature elimination
            a = 5, b = 2, c = 0, d = 5;
        }

        uint penalities = a * red + b * green_over_blue + c * green_over_green + d * smallFeature;
        if( penalities > blue ) penalities = penalities >> 1;
        pPop[curPop].value[currentMember] = blue - penalities;
    }

    currentMember--;

    // check for breed-phase
    if( currentMember == std::numeric_limits<uint>::max() )
    {
        auto coverage = float(red) / float(totalRed);

        if(iteration % 10 == 0) printf("%3d: %d %d %d %d %d -> %1.8f\n", iteration, blue, red, green_over_blue, green_over_green, smallFeature, coverage );

        if(
            (
                (coverage < coverageTarget || red < 10) &&
                red * green_over_blue < 100
            ) ||
            iteration == maxIter
        )
        { key('S',0,0); return; }

        uint16_t iBest;
        uint vBest, vSumPlus1;

        // scan current population to build stats
        // this included clobbering clones of iBest to prevent saturation-breeding
        pPop[curPop].scan( iBest, vBest, vSumPlus1 );

        // copy best-valued member to [0], from where we will use it to breed
        pPop[curPop].data[0].copy( pPop[curPop].data[iBest] );

        // preserve best into new population
        pPop[newPop].data[0].copy( pPop[curPop].data[0] );

        if(coverage > coverageTarget * 2 && iteration < maxIter * 1 / 10)
        {
            const int parts = 7;
            const int partN = popSize / parts;
            int part0 = 1, part1 = partN;

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].copy( pPop[curPop].data[0] ).join();

            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].copy( pPop[curPop].data[0] ).adjust( 5 );

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].copy( pPop[curPop].data[0] ).shift( 5 );

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].copy( pPop[curPop].data[0] ).random( 8 );

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].crossover(pPop[curPop], 0, uint(randValue() % vSumPlus1) );

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].crossover(pPop[curPop], uint(randValue() % vSumPlus1), uint(randValue() % vSumPlus1) );

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].randomize( 8 );
        }
        else
        {
            // finishing phase: minor position tweaking
            const int parts = 3;
            const int partN = popSize / parts;
            int part0 = 1, part1 = partN;

            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].copy( pPop[curPop].data[0] ).adjust( 2 );

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].copy( pPop[curPop].data[0] ).shift( 2 );

            part0 += partN, part1 += partN;
            for(int t=part0;t<part1;t++)
                pPop[newPop].data[t].copy( pPop[curPop].data[0] ).random( 4 );
        }

        currentMember = (popSize-1);
        iteration++;
    }

    glutPostRedisplay();
}

int main(int argc, char **argv)
{
    // https://www.freetype.org/freetype2/docs/tutorial/step1.html
    // https://github.com/benhj/glfreetype/blob/master/src/TextRenderer.cpp

    pPop = new Population[2];

    struct timespec spec;
    clock_gettime( CLOCK_REALTIME, &spec );
    srand((unsigned int)spec.tv_nsec);

    FT_CALL( FT_Init_FreeType( &ftLibrary ) );
    FT_CALL( FT_New_Face( ftLibrary, szFont, 0, &ftFace ) );
    FT_CALL( FT_Select_Charmap( ftFace, glyphCharmap) );
    FT_CALL( FT_Set_Pixel_Sizes( ftFace, 0, texDensity ) );

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_RGBA);
    glutInitWindowSize(viewDensity * 2,viewDensity);
    glutCreateWindow("Font test");

    glViewport(0,0,(GLsizei) viewDensity *2, (GLsizei) -viewDensity);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,viewDensity*2,0, viewDensity);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(1,-1,1);
    glTranslatef(0,-GLfloat(viewDensity),0);

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenTextures( sizeof(glyphHandle), &glyphHandle );
    texture();

    glutDisplayFunc(display);
    glutPostRedisplay();

    glutKeyboardFunc(key);
    glutIdleFunc(idle);

    pResultFile = fopen("results.txt", "a+t");
    fprintf( pResultFile, "#\n" );

    glutMainLoop();

    return(0);
}
