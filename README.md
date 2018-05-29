# gewellt-eight

'Gewellt' meaning 'wrinkly'... this code uses a genetic algorithim to optimize coverage of eight triangles
onto a font glyph. The glyph is drawn onto an opengl 1.3 texture which is then drawn onto a back buffer along with 
the candidate triangles. Alpha blending is used to determine coefficients for the objective function; a kind of
poor-man's opencl.

The genetic algorithim switches the weights on the objective function coefficients according to a simple
schedule. Combined with some unique operators (like position adjustment) this appears to simulate an
annealing or relaxation method.

When match tolerances become low enough, the match is saved to a .png and the triangles positions are
emmitted. With triangle cordinates being only byte-values this makes 48 bytes per glyph, plus one for
iteration count and one for the character code:

```
{ 77, '1', { 25,23,21,13,19,7,16,13,31,28,16,12,12,1,17,39,21,0,0,11,12,9,5,6,11,2,28,13,25,11,31,41,18,34,0,41,10,2,13,17,11,7,14,21,27,9,15,20,} }
{ 65, '2', { 17,37,3,3,10,20,7,34,26,16,28,14,23,36,22,36,19,33,24,21,18,0,27,6,1,8,21,3,8,0,3,43,0,37,29,39,29,0,22,11,24,8,26,13,0,37,8,36,} }
{ 65, '3', { 11,19,17,0,9,25,26,9,7,0,26,2,13,0,5,14,4,4,0,32,8,44,16,40,29,29,13,44,28,38,13,33,15,35,26,44,28,30,17,18,11,21,27,5,18,21,27,17,} }
{ 54, '4', { 25,0,12,10,10,16,0,33,0,27,25,29,13,27,29,41,12,26,19,30,29,27,22,42,15,34,26,0,22,12,21,0,19,33,26,21,16,9,6,28,1,26,21,29,4,41,24,27,} }
{ 52, '5', { 21,5,20,8,14,25,1,22,3,0,10,22,30,4,30,4,18,15,23,5,27,0,0,3,21,40,8,42,0,33,26,15,21,43,29,32,8,16,27,25,22,16,8,32,0,19,9,34,} }
{ 45, '6', { 26,33,8,25,19,30,4,18,27,22,19,15,27,4,17,4,24,11,7,7,8,0,25,2,0,18,7,2,4,43,26,34,9,41,21,42,25,40,20,17,29,28,4,38,10,42,6,31,} }
{ 51, '7', { 13,34,18,13,16,20,3,35,26,18,15,26,28,0,21,4,13,35,29,39,11,42,11,42,29,6,8,27,21,14,4,0,0,4,24,4,12,42,7,38,17,13,6,31,6,30,4,21,} }
{ 69, '8', { 0,36,11,44,26,40,5,20,0,31,5,39,10,14,12,22,14,30,27,19,17,0,27,5,6,7,0,13,8,21,30,35,22,13,23,40,7,1,0,10,22,1,9,17,23,20,5,25,} }
{ 62, '9', { 10,0,23,2,0,9,3,5,7,29,0,17,12,38,7,43,1,34,12,27,11,24,3,19,10,30,26,22,14,24,24,36,7,41,17,44,16,33,0,15,24,42,22,3,25,39,29,19,} }
```

![digit movie](https://github.com/orthopteroid/gewellt-eight/digits-8tri.gif "digit movie")