//
// FILE: gpreproc.h -- Implementation of gPreprocessor class,
//                     preprocessor for the command language input
//
// $Id$
//



#ifndef __gpreproc_h__
#define __gpreproc_h__


#include <assert.h>
#include <string.h>

#include "gcmdline.h"
#include "gstring.h"
#include "gstack.h"


class gPreprocessor
{
private:
  gCmdLineInput* m_CmdLine;

  gStack< gInput* > m_InputStack;
  gStack< int >     m_LineNumberStack;
  gStack< gString > m_FileNameStack;

  gString           m_PrevFileName;
  int               m_PrevLineNumber;

  gString           m_StartupString;



  gInput* LoadInput( gString& name );
  bool ExpectText( const char* text );
  
  bool EOL( char c ) const { return ( c == '\n' || c == '\r' ); }

  void SetPrompt( bool prompt )
  {
    if( m_InputStack.Peek() == m_CmdLine )
      m_CmdLine->SetPrompt( prompt );	  
  }

  void GetChar( char& c )
  {
    if( m_StartupString.length() > 0 )
    {
      c = m_StartupString[0];
      m_StartupString = m_StartupString.right( m_StartupString.length() - 1 );
    }
    else
    {
      assert( !eof() );
      m_InputStack.Peek()->get( c );
      if( EOL( c ) )
	++m_LineNumberStack.Peek();
    }
  }



public:

  gPreprocessor( gCmdLineInput* cmdline, const char* cmd = NULL ) 
    : m_CmdLine( cmdline ), m_PrevFileName( "console" ), m_PrevLineNumber( 1 ),
      m_StartupString( cmd )
  {
    assert( m_CmdLine );
    m_InputStack.Push( m_CmdLine );
    m_FileNameStack.Push( m_PrevFileName );
    m_LineNumberStack.Push( m_PrevLineNumber );

    if( !EOL( m_StartupString[ m_StartupString.length() - 1 ] ) )
      m_StartupString += '\n';
  }

  ~gPreprocessor()
  {
    while( m_InputStack.Depth() > 0 )
      delete m_InputStack.Pop();
  }


  //----------------------------------------------
  // eof
  //   If true, then there is no more input from
  //   the preprocessor.
  //-----------------------------------------------
  bool eof( void ) const { return m_InputStack.Depth() == 0; }


  //---------------------------------------------
  // GetLine
  //   Returns the preprocessed lines.  See 
  //   gpreproc.cc for details.
  //----------------------------------------------
  gString GetLine( void );



  //---------------------------------------------
  // GetFileName
  //   Returns the file name corresponding to
  //   the previous GetLine() call.
  //---------------------------------------------
  const gString& GetFileName( void ) const 
  { return m_PrevFileName; }
  

  //---------------------------------------------
  // GetLineNumber
  //   Returns the line number corresponding to
  //   the previous GetLine() call.
  //---------------------------------------------
  int GetLineNumber( void ) const 
  { return m_PrevLineNumber; }




};



#endif // __gpreproc_h__
