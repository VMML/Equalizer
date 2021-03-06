
/* Copyright (c) 2012-2014, Stefan Eilemann <eile@eyescale.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Eyescale Software GmbH nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <eq/eq.h>
#include <triply/modelTreeRoot.h>
#include <triply/treeGenerator.h>
#include <iostream>

namespace
{
static bool _isPlyfile( const std::string& filename )
{
    const size_t size = filename.length();
    if( size < 5 )
        return false;

    if( filename[size-4] != '.' || filename[size-3] != 'p' ||
        filename[size-2] != 'l' || filename[size-1] != 'y' )
    {
        return false;
    }
    return true;
}
}

int main( const int argc, char** argv )
{
    if( argc < 2 )
    {
        std::vector< std::string > names = triply::TreeGenerator::getRegisteredNames();
        auto nameIt = names.begin();
        std::cout << std::endl;
        std::cout << "Error: wrong number of arguments!" << std::endl << std::endl;
        std::cout << "Usage:    eqPlyConverter [partition:n] input_model(s)"<< std::endl;
        std::cout << std::endl;
        std::cout << "    - Available partition methods = " << *nameIt;
        while( ++nameIt != names.end() )
            std::cout << " | " << *nameIt;
        std::cout << std::endl;
        std::cout << "    - Default 'partition:n' = kd:2"<< std::endl;
        std::cout << std::endl;
        return EXIT_FAILURE;
    }

    triply::TreeInfo treeInfo = triply::TreeInfo( argv[1] );
    int firstModelArg = 2;
    if ( !treeInfo.isValid() )
    {
        // If no partition specified use binary KD tree
        treeInfo = triply::TreeInfo( "kd", 2 );
        firstModelArg = 1;
    }

    eq::Strings filenames;
    for( int i=firstModelArg; i < argc; ++i )
        filenames.push_back( argv[i] );

    while( !filenames.empty( ))
    {
        const std::string filename = filenames.back();
        filenames.pop_back();

        if( _isPlyfile( filename ))
        {
            triply::ModelTreeRoot* model = new triply::ModelTreeRoot;
            if( !model->readFromFile( filename.c_str( ), treeInfo, true ) )
                LBWARN << "Can't generate model: " << filename << std::endl;
            
            delete model;
        }
        else
        {
            const std::string basename = lunchbox::getFilename( filename );
            if( basename == "." || basename == ".." )
                continue;

            // recursively search directories
            const eq::Strings& subFiles =
                lunchbox::searchDirectory( filename, ".*" );

            for(eq::StringsCIter i = subFiles.begin(); i != subFiles.end(); ++i)
                filenames.push_back( filename + '/' + *i );
        }
    }
    return EXIT_SUCCESS;
}
