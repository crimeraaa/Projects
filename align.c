#include <stddef.h>		// offsetof
#include <stdalign.h>	// alignof
#include <stdio.h>		// printf

#define print_(a, b) printf(#a " = %zu, " #b " = %zu\n", a, b);

// https://stackoverflow.com/a/28464194
#define print(T) print_(alignof(T), offsetof(struct {char c; T d;}, d))

int main(void)
{
	size_t n = alignof(void *);
	print(char);
	print(short);
	print(int);
	print(long);
	print(long int);
	print(long long);
	print(long long int);
	print(float);
	print(double);
	print(long double);

	// unsigned types
	print(unsigned char);
	print(unsigned short);
	print(unsigned);
	print(unsigned int);
	print(unsigned long);
	print(unsigned long int);
	print(unsigned long long);
	print(unsigned long long int);

	// signed types
	print(signed char);
	print(signed short);
	print(signed);
	print(signed int);
	print(signed long);
	print(signed long int);
	print(signed long long);
	print(signed long long int);
	return 0;
}
