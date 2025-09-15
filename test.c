/*
 * File: polygon.c
 * Author: Saumya Debray
 * Purpose: Compute the area of a given regular n-gon
 */

/* includes commented out for scanner tests 
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
*/

/*
 * ngon_area(n, s) -- n is the number of sides of a regular polygon
 *	where each side is of length s; n is an int, s is a float.
 *	Returns a float that is the area of the polygon.
 *	n and s must be positive values.
 */
float ngon_area(int n, float s)
{
  float r /* apothem */, A  /* area */ ;

  if (n >= 3 && s > 0) {
    /*
     * the apothem, or radius of the inscribed circle, is given
     * by:
     *
     *	r = (s * cot(PI/n))/2 = (s * tan( PI/2 - (PI/n)))/2 
     */
    r = s * tan( M_PI_2 - M_PI/n )/2;
    /*
     * the area A of a regular polygon with n sides, each of length
     * s, is given by:
     *
     *	A = n * r^2 * tan (PI/n)
     */
    A = n * r * r * tan(M_PI/n);
    
    return A;
  }
}