/******************************************************************************
 Copyright (C) 2020 Maxim Samsonov                                      
 maxim@samsonov.net									

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom 
 the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included 
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "stdafx.h"
#include "ImageExtractor.h"

#ifdef _MSC_VER
#define snprintf _snprintf_s
#endif

ImageExtractor::ImageExtractor()
    : m_sOutputDirectory(), m_sInputFile(), m_nSuccess( 0 ), m_nCount( 0 ), m_sFName()
{
}

ImageExtractor::~ImageExtractor()
{
}

void ImageExtractor::Init(const char* pszInput, const char* pszOutput)
{
	if (!pszOutput || !pszInput)
	{
		PODOFO_RAISE_ERROR(ePdfError_InvalidHandle);
	}

	m_sOutputDirectory = pszOutput;
	m_sInputFile = pszInput;


	DWORD  dwGFA = GetFileAttributesA(pszOutput);

	if (dwGFA == INVALID_FILE_ATTRIBUTES) {
		if (!CreateDirectoryA(pszOutput, NULL)) {
			cerr << "Error: " << pszOutput << " does not exist and CreateDirectory failed." << endl << endl;
			exit(-1);
		}
	}
	else if (!(dwGFA & FILE_ATTRIBUTE_DIRECTORY)) {
		cerr << "Error: " << pszOutput << " exists but is not a directory." << endl << endl;
		exit(-1);
	}

	char drive[MAX_PATH];
	char dir[MAX_PATH];
	char name[MAX_PATH];
	char ext[MAX_PATH];
	_splitpath_s(m_sInputFile.c_str(), drive, MAX_PATH, dir, MAX_PATH, name, MAX_PATH, ext, MAX_PATH );

	m_sFName = name;
	
}


void ImageExtractor::Extract()
{

	PdfMemDocument document(m_sInputFile.c_str());
	int nPages = document.GetPageCount();

	const string kwdDo("Do");
	const PdfName nameXObject("XObject");

	for (int i = 0; i < nPages; i++) {

		cout << "Page " << setw(6) << (i + 1) << endl << flush;

		PdfPage* pPage = document.GetPage(i);
		PdfContentsTokenizer tokenizer(pPage);

		EPdfContentsType type;
		const char * kwText;
		PdfVariant var;
		bool bReadToken;

		PdfObject* pObj;
		PdfObject* pObjType;            // XoXo
		PdfObject* pObjSubType;			// XoXo
		PdfObject* pFilter;				// XoXo

		string theLastNameReadable("undefined");
		PdfVariant theLastName;

		unsigned int nCount = 0;
// --------------------------------------------------------------------------------------------------------------
// https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf 
// p. 201 and beyond
// We are looking for <XObject name> /Do  sequences
// --------------------------------------------------------------------------------------------------------------
		while ((bReadToken = tokenizer.ReadNext(type, kwText, var)))
		{
			switch (type) {
				case ePdfContentsType_Keyword: /* The token is a PDF keyword. */
					if (kwdDo == kwText) {
// Debug output			cout << "Do (XObject:\"" << setw(6) << theLastNameReadable << "\")" << endl;
						pObj = pPage->GetFromResources(nameXObject, theLastName.GetName());
						if (pObj && pObj->IsDictionary()) {
// Debug output				cout << "Dictionary ...";
							pObjType = pObj->GetDictionary().GetKey(PdfName::KeyType);
							pObjSubType = pObj->GetDictionary().GetKey(PdfName::KeySubtype);

							if ((pObjType && pObjType->IsName() && (pObjType->GetName().GetName() == "XObject")) ||
								(pObjSubType && pObjSubType->IsName() && (pObjSubType->GetName().GetName() == "Image")))
							{
// Debug output				    cout << " " << pObjType->GetName().GetName() << " and " << pObjSubType->GetName().GetName() << " ...";
								pFilter = pObj->GetDictionary().GetKey(PdfName::KeyFilter);
								if (pFilter && pFilter->IsArray() && pFilter->GetArray().GetSize() == 1 && pFilter->GetArray()[0].IsName())
									pFilter = &pFilter->GetArray()[0];


								string sFilterName = "";

								if (pFilter && pFilter->IsName())
									sFilterName = pFilter->GetName().GetName();


								if (sFilterName == "DCTDecode")
									ExtractImage(pObj, false, "jpg", i, nCount++);
								else if (sFilterName == "CCITTFaxDecode")
// https://coderoad.ru/2641770/%D0%98%D0%B7%D0%B2%D0%BB%D0%B5%D1%87%D0%B5%D0%BD%D0%B8%D0%B5-%D0%B8%D0%B7%D0%BE%D0%B1%D1%80%D0%B0%D0%B6%D0%B5%D0%BD%D0%B8%D1%8F-%D0%B8%D0%B7-PDF-%D1%81-%D0%BF%D0%BE%D0%BC%D0%BE%D1%89%D1%8C%D1%8E-%D1%84%D0%B8%D0%BB%D1%8C%D1%82%D1%80%D0%B0-CCITTFaxDecode
								ExtractImage(pObj, false, "tiff", i, nCount++);
								else if (sFilterName == "FlateDecode")
									ExtractImage(pObj, false, "ppm", i, nCount++);
								else
									cout << "* Filter \"" << sFilterName << "\" is not supported" << endl;

								document.FreeObjectMemory(pObj);
							}
						}
					}
					break;
				case ePdfContentsType_Variant: /* The token is a PDF variant. A variant is usually a parameter to a keyword */
				// Save the last Name token that can be a parameter for the following Do command
					if (var.IsName()) {
						theLastName = var;
						theLastName.ToString(theLastNameReadable);
// Debug output			cout << setw(6) << theLastNameReadable << "\" ... ";
					}
					break;
				case ePdfContentsType_ImageData: /* The "token" is raw inline image data found between ID and EI tags (see PDF ref section 4.8.6) */
					cout << "* Raw inline image data is not supported." << endl;
					break;
			};
		}
	}
	cout << "Done!" << endl << flush;
}


void ImageExtractor::ExtractImage( PdfObject* pObject, bool bDecode, string sExt, unsigned int nPage, unsigned int nCount)
{
	ofstream* pFStream = NULL;
	try 
	{
		pFStream = OpenFStream(sExt, nPage, nCount);
	}
	catch (...)  
	{
        PODOFO_RAISE_ERROR( ePdfError_InvalidHandle );
    }

    if( !bDecode ) 
    {
        PdfMemStream* pStream = dynamic_cast<PdfMemStream*>(pObject->GetStream());
        pFStream->write(pStream->Get(), pStream->GetLength());
    }
    else
    {
        //long lBitsPerComponent = pObject->GetDictionary().GetKey( PdfName("BitsPerComponent" ) )->GetNumber();
        // TODO: Handle colorspaces

        // Create a ppm image
        const char* pszPpmHeader = "P6\n# Image extracted by PoDoFo\n%" PDF_FORMAT_INT64 " %" PDF_FORMAT_INT64 "\n%li\n";
        
        

/*		fprintf( hFile, pszPpmHeader, 
                 pObject->GetDictionary().GetKey( PdfName("Width" ) )->GetNumber(),
                 pObject->GetDictionary().GetKey( PdfName("Height" ) )->GetNumber(),
                 255 );
                 
        char*    pBuffer;
        pdf_long lLen;
        pObject->GetStream()->GetFilteredCopy( &pBuffer, &lLen );
        fwrite( pBuffer, lLen, sizeof(char), hFile );
        free( pBuffer );
		*/
    }

	delete pFStream;

    ++m_nSuccess;
}


ofstream* ImageExtractor::OpenFStream(string sExt, unsigned int nPage, unsigned int nCount)
{
	string sFile;
	int nAttempt = 0;

	// Do not overwrite existing files:
	do {
		ostringstream ssFile;
		ssFile << m_sOutputDirectory << "/" << m_sFName << "-p-" << setw(3) << setfill('0') << nPage << "-i-" << setw(2) << nCount << "-a-" << setw(3) << nAttempt++ << "." << sExt;
		sFile = ssFile.str();
	} while (FileExists(sFile.c_str()));

	return new ofstream(sFile, ios_base::binary);
}


bool ImageExtractor::FileExists( const char* pszFilename )
{
    bool result = true;
    
    // if there is an error, it's probably because the file doesn't yet exist
    struct stat	stBuf;
    if ( stat( pszFilename, &stBuf ) == -1 )	result = false;

    return result;
}
