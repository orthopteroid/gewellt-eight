# gewellt-eight

'Gewellt' meaning 'wrinkly'... this code uses a genetic algorithim with OpenGL to optimize coverage of flat-shaded
 triangles onto a freetype-rendered font glyph. The approach creates scalable, compact and somewhat
 recognizable glyphs. Eight triangles are used, hence the name.

A freetype glyph-texture is drawn onto a back buffer along with alpha-blended candidate coverage triangles. The
 resulting render buffer is color-sampled to measure metrics of triangle coverage. These metrics 
are then used in the objective function to control breeding selection. As processing continues, the genetic
 algorithm uses a simple iteration-based schedule to switch the
 weights on the objective function coefficients which performs an annealing / relaxation on the triangle fitting. The
 texture
 size is 64x64, so the triangle verts only use 6 bits for their position information. This makes the 24 verts that
 make the triangles really only contain 36 bytes per glyph. Glyph metric information
 takes a few more bytes.

Below is a clip of the process. The left side are where candidate triangle coverage is tested and on the right side
 is where the best-so-far coverage is drawn. The code is set to only run 200 iterations per glyph and then save the
  drawing buffer to a .png and
  emmit the triangle positions. Here is a real-time video (taken from an old Core2 laptop) of
  some glyphs being fitted:

![digit movie](images/digits-8tri.gif)

The output after every glyph is a fixed-length structure comprising:

* the character-code (which I'm testing here in ascii, but gewellt plays fair with freetype to render utf8 glyphs)
* the freetype glyph metric information ()specifying dimensions, position and advance for horizontal and vertical layouts)
* the triangle data (gewellt only uses 64x64 glyph textures by default, so only a 6-bit coordinate system)

These are appended to a file:

```
{ '1', {31,42,4,42,38,-15,4,51,}, {17,33,31,41,0,41,0,11,0,7,4,11,21,28,14,34,14,12,8,11,8,11,18,10,1,11,2,6,1,8,0,8,13,4,7,11,19,0,11,0,20,26,7,0,17,0,11,0,} }
{ '2', {29,43,5,43,38,-14,4,51,}, {29,0,6,4,10,0,0,37,3,43,27,11,22,2,25,11,27,7,20,36,4,42,29,42,12,2,25,11,22,2,29,8,29,0,29,6,1,11,2,5,16,2,20,12,25,11,25,15,} }
{ '3', {30,44,4,43,38,-15,3,51,}, {10,2,15,2,8,3,17,4,6,3,2,8,27,16,28,7,20,17,30,28,12,44,26,40,5,7,10,7,0,11,30,10,22,1,7,1,29,30,20,17,10,21,0,32,14,41,8,42,} }
{ '4', {32,42,3,42,38,-16,4,51,}, {1,30,1,30,17,35,14,17,14,17,21,41,32,30,24,33,25,27,20,7,24,42,20,37,24,38,24,33,24,35,20,6,24,40,26,1,20,0,27,0,8,19,2,33,0,29,18,2,} }
{ '5', {30,43,4,42,38,-15,4,51,}, {30,37,25,17,21,21,7,18,14,27,1,10,0,31,3,39,19,43,10,16,5,22,25,17,27,30,13,42,27,38,8,6,26,3,23,0,5,24,7,6,2,11,22,0,3,0,2,9,} }
```

Individually, these glyphs look like this:

![digit](images/1.png)
![digit](images/2.png)
![digit](images/3.png)
![digit](images/4.png)
![digit](images/5.png)

Included in this repo is a simple single-line text editor demo that uses a limited gewellt-built glyph-set. OpenGL rendering of the
triangles is done in 'immediate mode' style - faster rendering would be possible through writing text-to-be
 rendered to a VBO:

![hello](images/hello-world.png)

# The Genetic Algorithim

The GA used has seven parts - each new generation is produced using the following steps:

* preserve best
* combine best with one random vertex join between the 8 triangles
* combine best with limited-distance random vertex position adjustments
* combine best with limited-distance random triangle shifts
* crossover the best with any likely candidate from the previous generation
* unconstrained crossover with any likely candidates
* combine the best with any 2 randomly generated triangles

The crossover approach performs substitution of a contiguous subsection from two mates.

The objective function is calculated as the initial amount of background blue (ie without any triangles) minus
weighted penalty values. After a triangle-set is drawn (in alpha) the penality values are summed from
a sampling of the drawing surface:

* red - the amount of glyph still showing
* green_over_blue - the amount of unnecessary triangle coverage on the background
* green_over_green - the amount of overlapping triangles (only possible because we use alpha to draw triangles)
* badTri - the number of triangles that fail a simple size-test by being too small

The penalties for these terms are typically applied in decreasing weight (ie, red has the higest priority) but
the code uses a schedule based upon the iteration count to swap the weights between the terms for periods
of 10 iterations at a time - this appears to create most of the annealing effects you see in the above gif.

# Future Directions

The results appear to be pretty rough - with many gaps, some outstanding overlap and some dead triangles. Some post
procesing could strip the dead triangles and possible link up nearby verticies (although there already is a 
mutation operator that is supposed to have done that...).

Other tricks might be to use 4 vert strips rather than just 3 very tris (effectively gaining an attached tri for
 the cost of just one vert) or to make gaps less perceptable by using triangle shading (although then the objective
 function might need some tweaking).

# Thanks to...

This project was inspired by
 [Roger Johansson's blog post](https://rogerjohansson.blog/2008/12/07/genetic-programming-evolution-of-mona-lisa/)
 concerning the triangularization of the Mona Lisa.
