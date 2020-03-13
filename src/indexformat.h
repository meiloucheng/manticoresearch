//
// Copyright (c) 2017-2020, Manticore Software LTD (http://manticoresearch.com)
// Copyright (c) 2001-2016, Andrew Aksyonoff
// Copyright (c) 2008-2016, Sphinx Technologies Inc
// All rights reserved
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//
#ifndef _indexformat_
#define _indexformat_

#include "sphinxstd.h"
#include "sphinxsearch.h"
#include "datareader.h"
#include "fileutils.h"

const int	DOCLIST_HINT_THRESH = 256;
const DWORD HITLESS_DOC_MASK = 0x7FFFFFFF;
const DWORD	HITLESS_DOC_FLAG = 0x80000000;

#define sphUnzipWordid sphUnzipOffset

class DiskIndexQwordSetup_c;

/// query word from the searcher's point of view
class DiskIndexQwordTraits_c : public ISphQword
{
public:
	/// tricky bit
	/// m_uHitPosition is always a current position in the .spp file
	/// base ISphQword::m_iHitlistPos carries the inlined hit data when m_iDocs==1
	/// but this one is always a real position, used for delta coding
	SphOffset_t		m_uHitPosition = 0;
	CSphMatch		m_tDoc;			///< current match (partial)

	FileBlockReaderPtr_c	m_rdDoclist;	///< my doclist accessor
	FileBlockReaderPtr_c	m_rdHitlist;	///< my hitlist accessor


					DiskIndexQwordTraits_c ( bool bUseMini, bool bExcluded );

	void			SetDocReader ( DataReaderFactory_c * pReader );
	void			SetHitReader ( DataReaderFactory_c * pReader );
	void			ResetDecoderState();
	virtual bool	Setup ( const DiskIndexQwordSetup_c * pSetup ) = 0;

protected:
	Hitpos_t		m_uInlinedHit {0};
	DWORD			m_uHitState = 0;
	Hitpos_t		m_iHitPos {EMPTY_HIT};	///< current hit postition, from hitlist

	static const int MINIBUFFER_LEN = 1024;
	BYTE			m_dHitlistBuf[MINIBUFFER_LEN];
	BYTE			m_dDoclistBuf[MINIBUFFER_LEN];

	BYTE *			m_pHitsBuf = nullptr;
	BYTE *			m_pDocsBuf = nullptr;

#ifndef NDEBUG
	bool			m_bHitlistOver = true;
#endif

};


struct CSphWordlistCheckpoint
{
	union
	{
		SphWordID_t		m_uWordID;
		const char *	m_sWord;
	};
	SphOffset_t			m_iWordlistOffset;
};


int DoclistHintUnpack ( DWORD uDocs, BYTE uHint );


// dictionary header
struct DictHeader_t
{
	int				m_iDictCheckpoints = 0;			///< how many dict checkpoints (keyword blocks) are there
	SphOffset_t		m_iDictCheckpointsOffset = 0;	///< dict checkpoints file position

	int				m_iInfixCodepointBytes = 0;		///< max bytes per infix codepoint (0 means no infixes)
	int64_t			m_iInfixBlocksOffset = 0;		///< infix blocks file position (stored as unsigned 32bit int as keywords dictionary is pretty small)
	int				m_iInfixBlocksWordsSize = 0;	///< infix checkpoints size
};


class CheckpointReader_c;

// FIXME: eliminate this, move it to proper dict impls
class CWordlist : public ISphWordlist, public DictHeader_t, public ISphWordlistSuggest
{
public:
	// !COMMIT slow data
	CSphMappedBuffer<BYTE>						m_tBuf;					///< my cache
	CSphFixedVector<CSphWordlistCheckpoint>		m_dCheckpoints {0};		///< checkpoint offsets


										~CWordlist () override;

	void								Reset();
	bool								Preread ( const CSphString & sName, bool bWordDict, int iSkiplistBlockSize, CSphString & sError );

	const CSphWordlistCheckpoint *		FindCheckpoint ( const char * sWord, int iWordLen, SphWordID_t iWordID, bool bStarMode ) const;
	bool								GetWord ( const BYTE * pBuf, SphWordID_t iWordID, CSphDictEntry & tWord ) const;

	const BYTE *						AcquireDict ( const CSphWordlistCheckpoint * pCheckpoint ) const;
	void								GetPrefixedWords ( const char * sSubstring, int iSubLen, const char * sWildcard, Args_t & tArgs ) const override;
	void								GetInfixedWords ( const char * sSubstring, int iSubLen, const char * sWildcard, Args_t & tArgs ) const override;

	void								SuffixGetChekpoints ( const SuggestResult_t & tRes, const char * sSuffix, int iLen, CSphVector<DWORD> & dCheckpoints ) const override;
	void								SetCheckpoint ( SuggestResult_t & tRes, DWORD iCP ) const override;
	bool								ReadNextWord ( SuggestResult_t & tRes, DictWord_t & tWord ) const override;
	int									GetWordsEnd() const { return m_iWordsEnd; }

	void								DebugPopulateCheckpoints();

private:
	bool								m_bWordDict = false;
	CSphVector<InfixBlock_t>			m_dInfixBlocks {0};
	CSphFixedVector<BYTE>				m_pWords {0};			///< arena for checkpoint's words
	BYTE *								m_pInfixBlocksWords = nullptr;	///< arena for infix checkpoint's words
	int									m_iSkiplistBlockSize {0};

	SphOffset_t							m_iWordsEnd = 0;		///< end of wordlist
	CheckpointReader_c *				m_pCpReader = nullptr;
};


/// dict=keywords block reader
class KeywordsBlockReader_c : public CSphDictEntry
{
public:
					KeywordsBlockReader_c ( const BYTE * pBuf, int iSkiplistBlockSize );

	void			Reset ( const BYTE * pBuf );
	bool			UnpackWord();

	const char *	GetWord() const			{ return (const char*)m_sWord; }
	int				GetWordLen() const		{ return m_iLen; }

private:
	const BYTE *	m_pBuf;
	BYTE			m_sWord [ MAX_KEYWORD_BYTES ];
	int				m_iLen;
	BYTE			m_uHint = 0;
	int				m_iSkiplistBlockSize = 0;
};


#endif // _indexformat_
