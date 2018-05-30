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
#include FT_FREETYPE_H

FT_Error ftError;
#define FT_CALL(x) do { ftError = (x); assert(!ftError); } while(false)

FT_Library ftLibrary;
FT_Face ftFace;

// when using -Wall unused return values make for chatty logs...
#define IGNORE_RESULT(fn) if (fn)

std::random_device randSeed;
std::default_random_engine randValue(randSeed());

const int resolution = 64; // a power of 2 <= 256, but 64 might be a more reasonable number
const int texDensity = resolution, texSize = texDensity * texDensity;
const int viewDensity = texDensity -1, viewSize = viewDensity * viewDensity;

const int popSize = 200;
const int numTris = 8;
const int numPoints = numTris * 3;
const int numValues = numPoints * 2;

struct Triangle { GLshort v[6]; }; // 6 values per tiangle
struct Population {
    Triangle data[ popSize ][ numTris ];
    uint value[ popSize ];
};
Population* pPop = 0;

std::uniform_int_distribution<GLshort> *pxDist = 0;
std::uniform_int_distribution<GLshort> *pyDist = 0;

//////////////////////////////////
// objective function state

// samplers
static GLubyte bufR[viewSize];
static GLubyte bufG[viewSize];
static GLubyte bufB[viewSize];

// statistics
uint blue; // background
uint red; // glyph
uint green; // triangle output
uint green_over_blue; // unnecessary triangle output on background
uint green_over_green; // unnecessary multi-layered triangle output
uint badTri;

///////////////////////////////////
// state vars

static const uint8_t* utf8GlyphsToProcess = (uint8_t*)u8"8"; // utf8 chars
static const FT_Encoding glyphCharmap = FT_ENCODING_UNICODE;

static uint8_t* glyphPtr = (uint8_t*)utf8GlyphsToProcess; // pointer to next utf8 char
static char sprintfBuffer[80];
static char utf8Buffer[4 +1]; // +1 for \0, static is not threadsafe

int currentMember = 0;
int iteration = 0;
FILE *pResultFile = 0;

GLubyte glyphWidth = 1;
GLubyte glyphRows = 1;
GLuint glyphHandle = 0;

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

inline void RANDPOS(GLshort* p, uint v)
{
    p[v] = GLshort(randValue() % ( (v & 1) ? glyphRows : glyphWidth) );
}

inline void ADJPOS(GLshort* p, uint v)
{
    p[v] = std::min<GLshort>(
        std::max<GLshort>(
            0,
            p[v] + GLshort(randValue() % 11) -5 // [-5,+5]
        ),
        GLshort( (v & 1) ? glyphRows : glyphWidth)
    );
}

inline void RANDOMTRI(GLshort* p)
{
    for( uint16_t v = 0; v < numValues / 2; v++ )
    {
        p[v * 2 + 0] = (*pxDist)(randValue);
        p[v * 2 + 1] = (*pyDist)(randValue);
    }
}

uint measure(std::function<uint(uint)> fn)
{
    uint m = 0;
    for(uint i=0;i<viewSize;i++) m += fn(i);
    return m;
}

void texture(bool dump = false)
{
    auto G = u8_composeLong( glyphPtr );
    auto glyph_index = FT_Get_Char_Index( ftFace, G );

    FT_CALL( FT_Load_Glyph( ftFace, glyph_index, FT_LOAD_DEFAULT ) );
    FT_CALL( FT_Render_Glyph( ftFace->glyph, FT_RENDER_MODE_NORMAL ) );

    glyphWidth = ftFace->glyph->bitmap.width;
    glyphRows = ftFace->glyph->bitmap.rows;

    if(pxDist) delete pxDist;
    pxDist = new std::uniform_int_distribution<GLshort>(0, glyphWidth);

    if(pyDist) delete pyDist;
    pyDist = new std::uniform_int_distribution<GLshort>(0, glyphRows);

    struct Texel { GLubyte l, a; }; // luminance & alpha
    static Texel texPixels[ texSize ];

    // Note: two channel bitmap (One for channel luminosity and one for alpha)
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

//////////////////////////////////////

static bool saverequest = false;
static bool mutaterequest = false;

void key(unsigned char c, int x, int y)
{
    uint curPop = iteration & 1;

    if(c=='r') // reset
    {
        currentMember = (popSize-1);
        iteration = 0;
    }

    if(c=='m') // mutate
    {
        mutaterequest = true;
    }

    if(c=='s')
    {
        saverequest = true;
    }

    if(c=='S')
    {
        snprintf(sprintfBuffer, sizeof(sprintfBuffer), "scrot \\%s.png -u", u8_composeString( glyphPtr ));
        IGNORE_RESULT(system((const char*) sprintfBuffer));
    }

    if(c=='n' || c=='S') // next
    {
        auto p = (GLshort*) &pPop[curPop].data[0];
        fprintf(pResultFile, "{ %d, '%s', { ", iteration, u8_composeString( glyphPtr ) );
        for(uint v=0;v<numValues;v++) fprintf(pResultFile, "%d,", p[v]);
        fprintf(pResultFile, "} }\n");

        glyphPtr += u8_charlength( glyphPtr );
        if(*glyphPtr == '\0')
            c = 27;
        else
        {
            currentMember = ( popSize - 1 );
            iteration = 0;

            randValue.seed(randSeed());
            texture();
        }
    }

    if(c == 27)
    {
        glDeleteTextures( sizeof( glyphHandle ), &glyphHandle );
        FT_Done_Face( ftFace );
        FT_Done_FreeType( ftLibrary );
        delete[] pPop;
        if(pxDist) delete pxDist;
        if(pyDist) delete pyDist;
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
        ::glReadPixels( 0, 0, viewDensity-1, viewDensity-1, GL_BLUE, GL_UNSIGNED_BYTE, bufB );

        blue = measure([&](uint i) { return bufB[i] > 0 ? 1 : 0; } );
    }

    // tris in alpha green
    glColor4f(0,1,0,.5f);
    glEnableClientState( GL_VERTEX_ARRAY );
    glVertexPointer( 2, GL_SHORT, 0, &pPop[curPop].data[currentMember] );
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
    glVertexPointer( 2, GL_SHORT, 0, &pPop[curPop].data[0] );
    glDrawArrays(GL_TRIANGLES, 0, numPoints);
    glDisableClientState( GL_VERTEX_ARRAY );

    glPopMatrix();

    ////////////////////////////////
    // sample

    glFlush();
    ::glReadPixels( 0, 0, viewDensity-1, viewDensity-1, GL_RED, GL_UNSIGNED_BYTE, bufR );
    ::glReadPixels( 0, 0, viewDensity-1, viewDensity-1, GL_GREEN, GL_UNSIGNED_BYTE, bufG );

    red = measure([&](uint i) { return bufR[i] > 250 ? 1 : 0; } );
    green = measure([&](uint i) { return bufG[i] > 0 ? 1 : 0; } );
    green_over_green = measure([&](uint i) { return bufG[i] > 180 ? 1 : 0; } ); // 180 is > 50%
    green_over_blue = measure([&](uint i) { return (bufG[i] & bufB[i]) ? 1 : 0; } );

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
        for( uint16_t j = 0; j < popSize; j++ ) RANDOMTRI( (GLshort *) &pPop[0].data[j] );
        for( uint16_t j = 0; j < popSize; j++ ) RANDOMTRI( (GLshort *) &pPop[1].data[j] );
        for( uint16_t j = 0; j < popSize; j++ ) pPop[0].value[j] = 0;
        for( uint16_t j = 0; j < popSize; j++ ) pPop[1].value[j] = 0;
    }
    else
    {
        // default objective function weights:
        // expose as much blue as possible while covering red and preventing triangle overlap
        uint a = 5, b = 3, c = 2, d = 1;

        // weight permutations allow annealing
        int batch = (iteration % 121) / 10 / 2; // [0,120] -> [0,12] in blocks of 10 -> [0,6] in blocks of 20
        switch( batch )
        {
            // put deflates together
            case 1: std::swap(a,b); break; // prefer background coverage to glyph coverage -> deflate
            case 2: std::swap(a,c); break; // perfer less triangle overlap to glyph coverage -> deflate
            // put inflates together
            case 3: std::swap(a,d); break; // perfer 'better triangles' to glyph coverage -> inflate
            case 4: std::swap(b,d); break; // perfer 'better triangles' to background coverage -> inflate
            default: ;
        }

        // quick check for tris that share cords - and are likely zero size
        badTri = 0;
        auto p = (GLshort*) &pPop[curPop].data[currentMember];
        for( uint16_t v = 1; v < numValues; v++ )
            badTri += (abs(p[v-1] - p[v]) < 2) ? 1 : 0;

        uint penalities = a * red + b * green_over_blue + c * green_over_green + d * badTri;
        if( penalities > blue ) penalities = penalities >> 1;
        pPop[curPop].value[currentMember] = blue - penalities;
    }

    currentMember--;

    // check for breed-phase
    if(currentMember == -1)
    {
        // check for termination and restart
        if(red < 40 && green_over_blue < 50 && badTri < 2) key('S',0,0); // pixel tolerances
        if(iteration > 100) key('r',0,0);

        // isolate best
        uint16_t bt = 0;
        uint bv = 0;
        {
            for( uint16_t t = 0; t < popSize; t++ )
            {
                if( pPop[curPop].value[t] > bv )
                    bv = pPop[curPop].value[bt = t];
            }
            auto pbt = (GLshort *) &pPop[curPop].data[bt];
            auto p0 = (GLshort *) &pPop[curPop].data[0];
            for( uint16_t v = 0; v < numValues; v++ ) p0[v] = pbt[v];
        }

        // clear best and clones
        for(uint16_t t=0;t<popSize;t++)
            if(pPop[curPop].value[t] == bv)
                pPop[curPop].value[t] = 0;

        // sum and exp-value
        uint sum = 0;
        for(uint16_t t=0;t<popSize;t++)
            sum += pPop[curPop].value[ t ];
        uint sump1 = sum +1; // +1 will put % operator in range of [0,sum]

        //////////////////////
        // crossover & mutation

        auto fnCrossover = [&](uint16_t t0, uint s1, uint s2)
        {
            // pick and switch
            uint16_t t1 = 0, t2 = 0;
            for(uint16_t tt=0;tt<popSize;tt++)
            {
                if(s1 > 0) { s1 -= pPop[curPop].value[ tt ]; t1 = tt; }
                if(s2 > 0) { s2 -= pPop[curPop].value[ tt ]; t2 = tt; }
            }
            if(randValue() & 1) std::swap( t1, t2 );

            // copy
            auto pt0 = (GLshort*) &pPop[newPop].data[t0];
            auto pt1 = (GLshort*) &pPop[curPop].data[t1];
            for( uint16_t v = 0; v < numValues; v++ ) pt0[v] = pt1[v];

            // merge
            auto pt2 = (GLshort*) &pPop[curPop].data[t2];
            auto start = uint16_t(randValue() % numValues);
            auto length = uint16_t(randValue() % (numValues - start));
            for( uint16_t v = start; v < (start+length); v++ ) pt0[v] = pt2[v];
        };

        // mutate curPop[0], the main source of the new population
        if(mutaterequest)
        {
            mutaterequest = false;

            auto p = (GLshort*) &pPop[curPop].data[0];
            for(uint16_t i=0;i<3; i++)
                RANDPOS(p, uint( randValue() % numValues ) );
        }

        // copy best
        {
            auto p0 = (GLshort*) &pPop[curPop].data[0];
            auto p = (GLshort*) &pPop[newPop].data[0];
            for( uint16_t v = 0; v < numValues; v++ ) p[v] = p0[v];
        }

        // permute best
        for(uint16_t t=1;t<popSize/4;t++)
        {
            auto p0 = (GLshort*) &pPop[curPop].data[0];
            auto p = (GLshort*) &pPop[newPop].data[t];
            for( uint16_t v = 0; v < numValues; v++ ) p[v] = p0[v];
            for(uint16_t i=0;i<3; i++)
                ADJPOS(p, uint(randValue() % numValues));
        }

        // breed best
        for(uint16_t t=popSize/4;t<popSize/2;t++)
            fnCrossover(t, 0, uint(randValue() % sump1) );

        // breed others
        for(uint16_t t=popSize/2;t<popSize*4/5;t++)
            fnCrossover(t, uint(randValue() % sump1), uint(randValue() % sump1) );

        // mutations
        for(uint16_t m=1;m<popSize / 20;m++)
        {
            auto t = (randValue() % (popSize*4/5 -1)) +1; // -1 includes range end, +1 skips [0]
            RANDPOS( (GLshort*) &pPop[newPop].data[t], randValue() % numValues );
        }

        // remainder are random
        for(uint16_t t=popSize*4/5;t<popSize;t++)
            RANDOMTRI( (GLshort *) &pPop[newPop].data[t] );

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
    FT_CALL( FT_New_Face( ftLibrary, "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf", 0, &ftFace ) );
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
    glutKeyboardFunc(key);
    glutIdleFunc(idle);

    pResultFile = fopen("results.txt", "wa");

    glutMainLoop();

    return(0);
}
