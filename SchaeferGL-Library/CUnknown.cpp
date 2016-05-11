/*
Copyright(c) 2016 Christopher Joseph Dean Schaefer

This software is provided 'as-is', without any express or implied
warranty.In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software.If you use this software
in a product, an acknowledgement in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "CUnknown.h"

CUnknown::CUnknown(void)
{

}

CUnknown::~CUnknown(void)
{

}

ULONG STDMETHODCALLTYPE CUnknown::AddRef(void)
{
	//TODO: Implement.

	return this->AddRef(0);
}

HRESULT STDMETHODCALLTYPE CUnknown::QueryInterface(REFIID riid,void  **ppv)
{
	//TODO: Implement.

	return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE CUnknown::Release(void)
{
	//TODO: Implement.

	return this->Release(0);
}

ULONG STDMETHODCALLTYPE CUnknown::AddRef(int which, char *comment)
{
	//TODO: Implement.

	return 0;
}

ULONG STDMETHODCALLTYPE	CUnknown::Release(int which, char *comment)
{
	//TODO: Implement.

	return 0;
}