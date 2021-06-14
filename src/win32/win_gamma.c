/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

/*
** WIN_GAMMA.C
*/
#include <assert.h>
#include "../renderer/tr_local.h"
#include "../qcommon/qcommon.h"
#include "glw_win.h"
#include "win_local.h"

static unsigned short s_oldHardwareGamma[3][256];

/*
** WG_CheckHardwareGamma
**
** Determines if the underlying hardware supports the Win32 gamma correction API.
*/
void WG_CheckHardwareGamma( void ) {
	HDC hDC;

	glConfig.deviceSupportsGamma = qfalse;

	if ( qwglSetDeviceGammaRamp3DFX ) {
		glConfig.deviceSupportsGamma = qtrue;

		hDC = GetDC( GetDesktopWindow() );
		glConfig.deviceSupportsGamma = qwglGetDeviceGammaRamp3DFX( hDC, s_oldHardwareGamma );
		ReleaseDC( GetDesktopWindow(), hDC );

		return;
	}

	// non-3Dfx standalone drivers don't support gamma changes, period
	if ( glConfig.driverType == GLDRV_STANDALONE ) {
	//	ri.Printf( PRINT_WARNING, "WG_CheckHardwareGamma: non-3Dfx Standalone driver, aborting\n" );
		return;
	}

	if ( !r_ignorehwgamma->integer ) {
	//	ri.Printf( PRINT_WARNING, "WG_CheckHardwareGamma: checking GetDeviceGammaRamp\n" );
		hDC = GetDC( GetDesktopWindow() );	// Knightmare- should be glw_state.hWnd?
		glConfig.deviceSupportsGamma = GetDeviceGammaRamp( hDC, s_oldHardwareGamma );
		ReleaseDC( GetDesktopWindow(), hDC );
	
		if ( glConfig.deviceSupportsGamma ) {
		//	ri.Printf( PRINT_WARNING, "WG_CheckHardwareGamma: found gamma ramp support\n" );
			//
			// do a sanity check on the gamma values
			//
			if ( ( HIBYTE( s_oldHardwareGamma[0][255] ) <= HIBYTE( s_oldHardwareGamma[0][0] ) ) ||
				 ( HIBYTE( s_oldHardwareGamma[1][255] ) <= HIBYTE( s_oldHardwareGamma[1][0] ) ) ||
				 ( HIBYTE( s_oldHardwareGamma[2][255] ) <= HIBYTE( s_oldHardwareGamma[2][0] ) ) ) {
				glConfig.deviceSupportsGamma = qfalse;
				ri.Printf( PRINT_WARNING, "WARNING: device has broken gamma support, generated gamma.dat\n" );
			}

			//
			// make sure that we didn't have a prior crash in the game, and if so we need to
			// restore the gamma values to at least a linear value
			//
			if ( ( HIBYTE( s_oldHardwareGamma[0][181] ) == 255 ) ) {
				int g;

				ri.Printf( PRINT_WARNING, "WARNING: suspicious gamma tables, using linear ramp for restoration\n" );

				for ( g = 0; g < 255; g++ )
				{
					s_oldHardwareGamma[0][g] = g << 8;
					s_oldHardwareGamma[1][g] = g << 8;
					s_oldHardwareGamma[2][g] = g << 8;
				}
			}
		}
	}
}

/*
void mapGammaMax( void ) {
	int		i, j;
	unsigned short table[3][256];

	// try to figure out what win2k will let us get away with setting
	for ( i = 0 ; i < 256 ; i++ ) {
		if ( i >= 128 ) {
			table[0][i] = table[1][i] = table[2][i] = 0xffff;
		} else {
			table[0][i] = table[1][i] = table[2][i] = i<<9;
		}
	}

	for ( i = 0 ; i < 128 ; i++ ) {
		for ( j = i*2 ; j < 255 ; j++ ) {
			table[0][i] = table[1][i] = table[2][i] = j<<8;
			if ( !SetDeviceGammaRamp( glw_state.hDC, table ) ) {
				break;
			}
		}
		table[0][i] = table[1][i] = table[2][i] = i<<9;
		Com_Printf( "index %i max: %i\n", i, j-1 );
	}
}
*/

/*
** GLimp_SetGamma
**
** This routine should only be called if glConfig.deviceSupportsGamma is TRUE
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] ) {
	unsigned short table[3][256];
	int i, j;
	int ret;
	OSVERSIONINFO vinfo;

	if ( !glConfig.deviceSupportsGamma || r_ignorehwgamma->integer || !glw_state.hDC ) {
		return;
	}

//mapGammaMax();

	for ( i = 0; i < 256; i++ ) {
		table[0][i] = ( ( ( unsigned short ) red[i] ) << 8 ) | red[i];
		table[1][i] = ( ( ( unsigned short ) green[i] ) << 8 ) | green[i];
		table[2][i] = ( ( ( unsigned short ) blue[i] ) << 8 ) | blue[i];
	}

	// Win2K puts this odd restriction on gamma ramps...
	vinfo.dwOSVersionInfoSize = sizeof( vinfo );
	GetVersionEx( &vinfo );
	// Knightmare- change from == 5 to >= 5 (Win Vista & newer)
	if ( vinfo.dwMajorVersion >= 5 && vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT ) {
	//	ri.Printf( PRINT_WARNING, "GLimp_SetGamma: performing W2K gamma clamp.\n" );
		Com_DPrintf( "GLimp_SetGamma: performing W2K gamma clamp.\n" );
		for ( j = 0 ; j < 3 ; j++ ) {
			for ( i = 0 ; i < 128 ; i++ ) {
				if ( table[j][i] > ( ( 128 + i ) << 8 ) ) {
					table[j][i] = ( 128 + i ) << 8;
				}
			}
			if ( table[j][127] > 254 << 8 ) {
				table[j][127] = 254 << 8;
			}
		}
	} else {
		Com_DPrintf( "GLimp_SetGamma: skipping W2K gamma clamp.\n" );
	}

	// enforce constantly increasing
	for ( j = 0 ; j < 3 ; j++ ) {
		for ( i = 1 ; i < 256 ; i++ ) {
			if ( table[j][i] < table[j][i - 1] ) {
				table[j][i] = table[j][i - 1];
			}
		}
	}


	if ( qwglSetDeviceGammaRamp3DFX ) {
		qwglSetDeviceGammaRamp3DFX( glw_state.hDC, table );
	}
	else {
		ret = SetDeviceGammaRamp( glw_state.hDC, table );
		if ( !ret ) {
			Com_DPrintf( "GLimp_SetGamma: SetDeviceGammaRamp failed.\n" );
		}
	}
}

/*
** WG_RestoreGamma
*/
void WG_RestoreGamma( void ) {
	if ( glConfig.deviceSupportsGamma ) {
		if ( qwglSetDeviceGammaRamp3DFX ) {
			qwglSetDeviceGammaRamp3DFX( glw_state.hDC, s_oldHardwareGamma );
		}
		else {
			HDC hDC;

			hDC = GetDC( GetDesktopWindow() );
			SetDeviceGammaRamp( hDC, s_oldHardwareGamma );
			ReleaseDC( GetDesktopWindow(), hDC );
		}
	}
}

