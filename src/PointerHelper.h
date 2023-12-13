#pragma once

#ifndef TRY_GET_PTR
#define TRY_GET_PTR(x, m) (x) ? (x->m) : nullptr;
#endif // !TRY_GET_PTR
