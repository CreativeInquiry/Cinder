/*
 Copyright (c) 2014, The Cinder Project, All rights reserved.

 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#if defined( CINDER_GL_ANGLE )

#include "cinder/app/msw/RendererImplGlAngle.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Log.h"

#include "cinder/app/AppBase.h"
#include "cinder/gl/Environment.h"
#include <signal.h>

#if defined( CINDER_UWP )
	#include "cinder/winrt/WinRTUtils.h"
	#include "angle_windowsstore.h"
#endif

#define DEBUG_GL 1

#if ! defined( CI_SCREENSAVER )
#define CI_BREAK() { __debugbreak(); }
//#define CI_BREAK() { __asm int 3; }
#else
#define CI_BREAK() 
#endif

using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Microsoft::WRL;
using namespace Platform;

namespace cinder { namespace app {

void checkGlStatus();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RendererImplGlAngle
RendererImplGlAngle::RendererImplGlAngle( RendererGl *renderer )
	: mRenderer( renderer )
{
	
}

void RendererImplGlAngle::CleanupEGL()
{
	if (mDisplay!= EGL_NO_DISPLAY && mSurface != EGL_NO_SURFACE)
	{
		eglDestroySurface(mDisplay, mSurface);
		mSurface = EGL_NO_SURFACE;
	}

	if (mDisplay != EGL_NO_DISPLAY && mContext != EGL_NO_CONTEXT)
	{
		eglDestroyContext(mDisplay, mContext);
		mContext = EGL_NO_CONTEXT;
	}

	if (mDisplay != EGL_NO_DISPLAY)
	{
		eglTerminate(mDisplay);
		mDisplay = EGL_NO_DISPLAY;
	}
}


bool RendererImplGlAngle::initialize(Windows::Graphics::Holographic::HolographicSpace^ holographicSpace, RendererRef sharedRenderer)
{
	 const EGLint configAttributes[] = 
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    const EGLint contextAttributes[] = 
    { 
        EGL_CONTEXT_CLIENT_VERSION, 2, 
        EGL_NONE
    };

    const EGLint surfaceAttributes[] =
    {
        // EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is part of the same optimization as EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER (see above).
        // If you have compilation issues with it then please update your Visual Studio templates.
        EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        EGL_NONE
    };

    const EGLint defaultDisplayAttributes[] =
    {
        // These are the default display attributes, used to request ANGLE's D3D11 renderer.
        // eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

        // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER is an optimization that can have large performance benefits on mobile devices.
        // Its syntax is subject to change, though. Please update your Visual Studio templates if you experience compilation issues with it.
        EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        
        // EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that enables ANGLE to automatically call 
        // the IDXGIDevice3::Trim method on behalf of the application when it gets suspended. 
        // Calling IDXGIDevice3::Trim when an application is suspended is a Windows Store application certification requirement.
        EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
        EGL_NONE,
    };
    
    const EGLint fl9_3DisplayAttributes[] =
    {
        // These can be used to request ANGLE's D3D11 renderer, with D3D11 Feature Level 9_3.
        // These attributes are used if the call to eglInitialize fails with the default display attributes.
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
        EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
        EGL_NONE,
    };

    const EGLint warpDisplayAttributes[] =
    {
        // These attributes can be used to request D3D11 WARP.
        // They are used if eglInitialize fails with both the default display attributes and the 9_3 display attributes.
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
        EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
        EGL_NONE,
    };
    
    EGLConfig config = NULL;

    // eglGetPlatformDisplayEXT is an alternative to eglGetDisplay. It allows us to pass in display attributes, used to configure D3D11.
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!eglGetPlatformDisplayEXT)
    {
        throw Exception::Exception("Failed to get function eglGetPlatformDisplayEXT");
    }

    //
    // To initialize the display, we make three sets of calls to eglGetPlatformDisplayEXT and eglInitialize, with varying 
    // parameters passed to eglGetPlatformDisplayEXT:
    // 1) The first calls uses "defaultDisplayAttributes" as a parameter. This corresponds to D3D11 Feature Level 10_0+.
    // 2) If eglInitialize fails for step 1 (e.g. because 10_0+ isn't supported by the default GPU), then we try again 
    //    using "fl9_3DisplayAttributes". This corresponds to D3D11 Feature Level 9_3.
    // 3) If eglInitialize fails for step 2 (e.g. because 9_3+ isn't supported by the default GPU), then we try again 
    //    using "warpDisplayAttributes".  This corresponds to D3D11 Feature Level 11_0 on WARP, a D3D11 software rasterizer.
    //
    
    // This tries to initialize EGL to D3D11 Feature Level 10_0+. See above comment for details.
    mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
    if (mDisplay == EGL_NO_DISPLAY)
    {
        throw Exception::Exception("Failed to get EGL display");
    }

    if (eglInitialize(mDisplay, NULL, NULL) == EGL_FALSE)
    {
        // This tries to initialize EGL to D3D11 Feature Level 9_3, if 10_0+ is unavailable (e.g. on some mobile devices).
        mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes);
        if (mDisplay == EGL_NO_DISPLAY)
        {
            throw Exception::Exception("Failed to get EGL display");
        }

        if (eglInitialize(mDisplay, NULL, NULL) == EGL_FALSE)
        {
            // This initializes EGL to D3D11 Feature Level 11_0 on WARP, if 9_3+ is unavailable on the default GPU.
            mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, warpDisplayAttributes);
            if (mDisplay == EGL_NO_DISPLAY)
            {
                throw Exception::Exception("Failed to get EGL display");
            }

            if (eglInitialize(mDisplay, NULL, NULL) == EGL_FALSE)
            {
                // If all of the calls to eglInitialize returned EGL_FALSE then an error has occurred.
                throw Exception::Exception("Failed to initialize EGL");
            }
        }
    }

    EGLint numConfigs = 0;
    if ((eglChooseConfig(mDisplay, configAttributes, &config, 1, &numConfigs) == EGL_FALSE) || (numConfigs == 0))
    {
        throw Exception::Exception("Failed to choose first EGLConfig");
    }

    // Create a PropertySet and initialize with the EGLNativeWindowType.
    PropertySet^ surfaceCreationProperties = ref new PropertySet();
    //surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), window);
    surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), holographicSpace);
    if (mStationaryReferenceFrame != nullptr)
    {
        surfaceCreationProperties->Insert(ref new String(EGLBaseCoordinateSystemProperty), mStationaryReferenceFrame);
    }

    // You can configure the surface to render at a lower resolution and be scaled up to
    // the full window size. This scaling is often free on mobile hardware.
    //
    // One way to configure the SwapChainPanel is to specify precisely which resolution it should render at.
    // Size customRenderSurfaceSize = Size(800, 600);
    // surfaceCreationProperties->Insert(ref new String(EGLRenderSurfaceSizeProperty), PropertyValue::CreateSize(customRenderSurfaceSize));
    //
    // Another way is to tell the SwapChainPanel to render at a certain scale factor compared to its size.
    // e.g. if the SwapChainPanel is 1920x1280 then setting a factor of 0.5f will make the app render at 960x640
    // float customResolutionScale = 0.5f;
    // surfaceCreationProperties->Insert(ref new String(EGLRenderResolutionScaleProperty), PropertyValue::CreateSingle(customResolutionScale));

    mSurface = eglCreateWindowSurface(mDisplay, config, reinterpret_cast<IInspectable*>(surfaceCreationProperties), surfaceAttributes);
    if (mSurface == EGL_NO_SURFACE)
    {
        throw Exception::Exception("Failed to create EGL fullscreen surface");
    }

    mContext = eglCreateContext(mDisplay, config, EGL_NO_CONTEXT, contextAttributes);
    if (mContext == EGL_NO_CONTEXT)
    {
        throw Exception::Exception("Failed to create EGL context");
    }

    if (eglMakeCurrent(mDisplay, mSurface, mSurface, mContext) == EGL_FALSE)
    {
        throw Exception::Exception("Failed to make fullscreen EGLSurface current");
    }

	return true;
}


#if defined( CINDER_MSW_DESKTOP )
bool RendererImplGlAngle::initialize( HWND wnd, HDC dc, RendererRef sharedRenderer )
#elif defined( CINDER_UWP )
bool RendererImplGlAngle::initialize( ::Platform::Agile<Windows::UI::Core::CoreWindow> wnd, RendererRef sharedRenderer )
#endif
{
	mWnd = wnd;
#if defined( CINDER_MSW_DESKTOP )
	mDc = dc;
#endif

	if( sharedRenderer )
		CI_LOG_E( "ANGLE does not currenty supported shared renderers." );

	std::vector<EGLint> configAttribs;
	configAttribs.push_back( EGL_RED_SIZE ); configAttribs.push_back( 8 );
	configAttribs.push_back( EGL_GREEN_SIZE ); configAttribs.push_back( 8 );
	configAttribs.push_back( EGL_BLUE_SIZE ); configAttribs.push_back( 8 );
	configAttribs.push_back( EGL_ALPHA_SIZE ); configAttribs.push_back( EGL_DONT_CARE );
	configAttribs.push_back( EGL_DEPTH_SIZE ); configAttribs.push_back( mRenderer->getOptions().getDepthBufferDepth() );
	configAttribs.push_back( EGL_STENCIL_SIZE ); configAttribs.push_back( mRenderer->getOptions().getStencil() ? 8 : EGL_DONT_CARE );
// multisampling doesn't currently appear to work
//	configAttribs.push_back( EGL_SAMPLE_BUFFERS ); configAttribs.push_back( 1 );
	configAttribs.push_back( EGL_NONE );

	EGLint surfaceAttribList[] = {
		EGL_NONE, EGL_NONE
	};

	PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>( eglGetProcAddress("eglGetPlatformDisplayEXT") );
	if( ! eglGetPlatformDisplayEXT )
		return false;

#if defined( CINDER_MSW_DESKTOP )
	const EGLint displayAttributes[] = {
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
		EGL_NONE
	};

	mDisplay = eglGetPlatformDisplayEXT( EGL_PLATFORM_ANGLE_ANGLE, dc, displayAttributes );
#else
	const EGLint displayAttributes[] = {
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
		EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE, 
		EGL_NONE,
	};

	mDisplay = eglGetPlatformDisplayEXT( EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttributes );
#endif
	if( mDisplay == EGL_NO_DISPLAY )
		return false;

	EGLint majorVersion, minorVersion;
	if( eglInitialize( mDisplay, &majorVersion, &minorVersion ) == EGL_FALSE ) {
#if defined( CINDER_MSW_DESKTOP )
		// try again with D3D11 Feature Level 9.3 if 10.0+ is unavailable
		const EGLint fl9_3DisplayAttributes[] = {
			EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
			EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
			EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
			EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
			EGL_NONE,
		};

		mDisplay = eglGetPlatformDisplayEXT( EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes );
		if( mDisplay == EGL_NO_DISPLAY )
			return false;

		if( eglInitialize( mDisplay, &majorVersion, &minorVersion ) == EGL_FALSE ) {
			// NB: could try again with D3D11 Feature Level 11.0 on WARP if 9.3+ is unavailable
			return false;
		}
#else
		return false;
#endif
	}

	eglBindAPI( EGL_OPENGL_ES_API );
	if( eglGetError() != EGL_SUCCESS )
		return false;

	EGLint configCount;
	EGLConfig config;
	if( ! eglChooseConfig( mDisplay, configAttribs.data(), &config, 1, &configCount ) || (configCount != 1) )
		return false;

#if defined( CINDER_MSW_DESKTOP )
	mSurface = eglCreateWindowSurface( mDisplay, config, wnd, NULL );
#else
	Windows::Foundation::Collections::PropertySet^ surfaceCreationProperties = ref new Windows::Foundation::Collections::PropertySet();
	surfaceCreationProperties->Insert( ref new ::Platform::String(EGLNativeWindowTypeProperty), wnd.Get() );

	mSurface = eglCreateWindowSurface( mDisplay, config, reinterpret_cast<IInspectable*>(surfaceCreationProperties), NULL );
#endif
 
	auto err = eglGetError();
	if( err != EGL_SUCCESS )
		return false;

	EGLint contextAttibutes[] = {
#if defined( CINDER_GL_ES_3 )
		EGL_CONTEXT_CLIENT_VERSION, 3,
#else
		EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
		EGL_NONE
	};

	mContext = eglCreateContext( mDisplay, config, EGL_NO_CONTEXT, contextAttibutes );
	if( mContext == EGL_NO_CONTEXT )
		return false;
	checkGlStatus();

	eglMakeCurrent( mDisplay, mSurface, mSurface, mContext );
	if( eglGetError() != EGL_SUCCESS )
		return false;
	checkGlStatus();

	gl::Environment::setEs();
	checkGlStatus();

	std::shared_ptr<gl::PlatformDataAngle> platformData( new gl::PlatformDataAngle( mContext, mDisplay, mSurface, config ) );
	platformData->mObjectTracking = mRenderer->getOptions().getObjectTracking();

	mCinderContext = gl::Context::createFromExisting( platformData );
	checkGlStatus();

	mCinderContext->makeCurrent();

	checkGlStatus();

	eglSwapInterval( mDisplay, 0 );
	checkGlStatus();

	return true;
}

void RendererImplGlAngle::kill()
{
}

void RendererImplGlAngle::prepareToggleFullScreen()
{
//	mImpl->prepareToggleFullScreen();
}

void RendererImplGlAngle::finishToggleFullScreen()
{
//	mImpl->finishToggleFullScreen();
}

void RendererImplGlAngle::defaultResize() const
{
	checkGlStatus();

#if defined( CINDER_MSW_DESKTOP )
	::RECT clientRect;
	::GetClientRect( mWnd, &clientRect );
	int width = clientRect.right - clientRect.left;
	int height = clientRect.bottom - clientRect.top;
#else
	float widthf, heightf;
	winrt::GetPlatformWindowDimensions( mWnd.Get(), &widthf, &heightf );
	int width = (int)widthf;
	int height = (int)heightf;
#endif

	gl::viewport( 0, 0, width, height );
	gl::setMatricesWindow( width, height );
}

void RendererImplGlAngle::swapBuffers() const
{
	auto status = ::eglMakeCurrent( mDisplay, mSurface, mSurface, mContext );
	assert( status );
	EGLBoolean result = ::eglSwapBuffers( mDisplay, mSurface );
	assert( result );
}

void RendererImplGlAngle::makeCurrentContext( bool force )
{
	mCinderContext->makeCurrent( force );
}

EGLint getEglError()
{
	return eglGetError();
}

std::string getEglErrorString( EGLint err )
{
	switch( err ) {
		case EGL_SUCCESS: return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
		default: return "unknown error code";
	}
}

void checkGlStatus()
{
#if DEBUG_GL
	EGLint lastEglError = getEglError();
	if( lastEglError != EGL_SUCCESS ) {
		ci::app::console() << "EGL ERROR: " << getEglErrorString( lastEglError ) << std::endl;
		CI_BREAK();
	}

	GLenum lastGlError = ci::gl::getError();
	if( lastGlError != GL_NO_ERROR ) {
		ci::app::console() << "GL ERROR: " << ci::gl::getErrorString( lastGlError ) << std::endl;
	}
#endif // DEBUG_GL
}

} } // namespace cinder::app


#endif // defined( CINDER_GL_ANGLE )
