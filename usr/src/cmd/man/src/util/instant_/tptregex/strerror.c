
/* standin for strerror(3) which is missing on some systems
 * (eg, SUN)
 */
#pragma ident   "@(#)strerror.c 1.2     97/03/07 SMI"

char *
strerror(int num)
{
	perror(num);
	return "";
}    
