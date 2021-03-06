//	Multiply rational numbers
//
//	Input:		tos-2		multiplicand
//
//			tos-1		multiplier
//
//	Output:		product on stack

#include "stdafx.h"
#include "defs.h"

void
qmul(void)
{
	unsigned int *aa, *bb, *c;

	save();

	p2 = pop();
	p1 = pop();

	// zero?

	if (MZERO(p1->u.q.a) || MZERO(p2->u.q.a)) {
		push(zero);
		restore();
		return;
	}

	aa = mmul(p1->u.q.a, p2->u.q.a);
	bb = mmul(p1->u.q.b, p2->u.q.b);

	c = mgcd(aa, bb);

	MSIGN(c) = MSIGN(bb);

	p1 = alloc();

	p1->k = NUM;

	p1->u.q.a = mdiv(aa, c);
	p1->u.q.b = mdiv(bb, c);

	mfree(aa);
	mfree(bb);

	push(p1);

	restore();
}
