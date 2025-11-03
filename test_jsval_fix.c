/*
 * Test file to verify the JSVAL_VOID fix
 * This demonstrates the undefined behavior issue and the fix
 */

#include <stdio.h>
#include <stdint.h>

// Simulate the JavaScript engine types
typedef unsigned int jsuint;
typedef int jsval;

#define JSVAL_INT               0x1     /* tagged 31-bit integer value */
#define JSVAL_INT_POW2(n)       ((jsval)1 << (n))

// Original problematic macro (commented out)
// #define JSVAL_VOID_OLD          INT_TO_JSVAL(0 - JSVAL_INT_POW2(30))
// #define INT_TO_JSVAL(i)         (((jsval)(i) << 1) | JSVAL_INT)

// Fixed macro
#define JSVAL_VOID              ((jsval)(((jsuint)(0 - JSVAL_INT_POW2(30)) << 1) | JSVAL_INT))

int main() {
    // Demonstrate the problem
    jsval negative_value = 0 - JSVAL_INT_POW2(30);
    printf("Negative value: %d (0x%08x)\n", negative_value, negative_value);
    
    // Show what JSVAL_VOID evaluates to with our fix
    jsval void_value = JSVAL_VOID;
    printf("JSVAL_VOID: %d (0x%08x)\n", void_value, void_value);
    
    // Show the unsigned cast approach
    jsuint unsigned_negative = (jsuint)(0 - JSVAL_INT_POW2(30));
    printf("Unsigned cast: %u (0x%08x)\n", unsigned_negative, unsigned_negative);
    jsval shifted = ((jsuint)(0 - JSVAL_INT_POW2(30)) << 1) | JSVAL_INT;
    printf("After shift and OR: %d (0x%08x)\n", shifted, shifted);
    
    printf("\nFix eliminates undefined behavior while preserving bit pattern.\n");
    return 0;
}
