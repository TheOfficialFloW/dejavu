#include <stddef.h>

unsigned int strlen(const char *s)
{
	int len = 0;
	while(*s)
	{
		s++;
		len++;
	}

	return len;
}

char *strcat(char *s, const char *append)
{
	char *pRet = s;
	while(*s)
	{
		s++;
	}

	while(*append)
	{
		*s++ = *append++;
	}

	*s = 0;

	return pRet;
}

char *strncat(char *s, const char *append, size_t count)
{
	char *pRet = s;

	while(*s)
	{
		s++;
	}

	while((*append) && (count > 0))
	{
		*s++ = *append++;
		count--;
	}

	*s = 0;

	return pRet;
}

int strcmp(const char *s1, const char *s2)
{
	int val = 0;
	const unsigned char *u1, *u2;

	u1 = (unsigned char *) s1;
	u2 = (unsigned char *) s2;

	while(1)
	{
		if(*u1 != *u2)
		{
			val = (int) *u1 - (int) *u2;
			break;
		}

		if((*u1 == 0) && (*u2 == 0))
		{
			break;
		}

		u1++;
		u2++;
	}

	return val;
}

int strncmp(const char *s1, const char *s2, size_t count)
{
	int val = 0;
	const unsigned char *u1, *u2;

	u1 = (unsigned char *) s1;
	u2 = (unsigned char *) s2;

	while(count > 0)
	{
		if(*u1 != *u2)
		{
			val = (int) *u1 - (int) *u2;
			break;
		}

		if((*u1 == 0) && (*u2 == 0))
		{
			break;
		}

		u1++;
		u2++;
		count--;
	}

	return val;
}

char *strcpy(char *dst, const char *src)
{
	char *pRet = dst;

	while(*src)
	{
		*dst++ = *src++;
	}

	*dst = 0;

	return pRet;
}

char *strncpy(char *dst, const char *src, size_t count)
{
	char *pRet = dst;

	while(count > 0)
	{
		if(*src)
		{
			*dst++ = *src++;
		}
		else
		{
			*dst++ = 0;
		}

		count--;
	}

	return pRet;
}

int memcmp(const void *b1, const void *b2, size_t len)
{
	int val = 0;
	const unsigned char *u1, *u2;

	u1 = (const unsigned char *) b1;
	u2 = (const unsigned char *) b2;

	while(len > 0)
	{
		if(*u1 != *u2)
		{
			val = (int) *u1 - (int) *u2;
			break;
		}
		u1++;
		u2++;
		len--;
	}

	return val;
}

void *memcpy(void *dst, const void *src, size_t len)
{
	void *pRet = dst;
	const char *usrc = (const char *) src;
	char *udst = (char *) dst;

	while(len > 0)
	{
		*udst++ = *usrc++;
		len--;
	}

	return pRet;
}

void *memmove(void *dst, const void* src, size_t len)
{
	void *pRet = dst;
	char *udst;
	const char *usrc;

	if(dst < src)
	{
		/* Copy forwards */
		udst = (char *) dst;
		usrc = (const char *) src;
		while(len > 0)
		{
			*udst++ = *usrc++;
			len--;
		}
	}
	else
	{
		/* Copy backwards */
		udst = ((char *) dst) + len;
		usrc = ((const char *) src) + len;
		while(len > 0)
		{
			*--udst = *--usrc;
			len--;
		}
	}
	
	return pRet;
}

void *memset(void *b, int c, size_t len)
{
	void *pRet = b;
	unsigned char *ub = (unsigned char *) b;

	while(len > 0)
	{
		*ub++ = (unsigned char) c;
		len--;
	}

	return pRet;
}
