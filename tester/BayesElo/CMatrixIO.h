// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

/////////////////////////////////////////////////////////////////////////////
//
// Rémi Coulom
//
// February, 2005
//
/////////////////////////////////////////////////////////////////////////////
#ifndef CMatrixIO_Declared
#define CMatrixIO_Declared

#include <iosfwd>

class CMatrix;

std::ostream& operator<<(std::ostream& out, const CMatrix& m);

#endif  // CMatrixIO_Declared
