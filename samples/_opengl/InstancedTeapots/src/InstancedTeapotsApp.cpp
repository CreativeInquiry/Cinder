#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class BasicApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void resize() override;
	void update() override;
	void draw() override;

private:


	double				mTime;

	CameraPersp			mCamera;

	gl::GlslProgRef		mShader;
	gl::BatchRef		mTeapot;

	vec3				mViewerPosition;
	vec4				mLightWorldPosition;

	// Only applies to HTC Vive with standing tracking origin
	bool				mRecalcOrigin = false;

	void drawScene();
};

void BasicApp::setup()
{
	mCamera.lookAt(ci::vec3(0, 0, 3), ci::vec3(0, 0, 0));

	mShader = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("shader_es3.vert")).fragment(loadAsset("shader_es3.frag")));
	mTeapot = gl::Batch::create(geom::Teapot().subdivisions(12), mShader);

	gl::disableAlphaBlending();
	gl::enableDepthRead();
	gl::enableDepthWrite();

}

void BasicApp::mouseDown(MouseEvent event)
{
}

void BasicApp::resize()
{
	mCamera.setAspectRatio(getWindowAspectRatio());
}

void BasicApp::update()
{
	// Animate light position.
	mTime = getElapsedSeconds();
	float t = float(mTime) * 0.4f;
	mLightWorldPosition = vec4(math<float>::sin(t), math<float>::sin(t * 4.0f), math<float>::cos(t), 1);
}

void BasicApp::drawScene()
{
	gl::enableDepthRead();
	gl::enableDepthWrite();

	{
		gl::ScopedModelMatrix push;
		gl::rotate((float)mTime, vec3(-0.3f, -1.0f, 0.2f));
		gl::scale(vec3(0.5f));
		mTeapot->draw(2);
	}

	gl::lineWidth(3.0f);
	gl::drawCoordinateFrame(2);
	gl::drawSphere(vec3(mLightWorldPosition), 0.05f, 36);
}

void BasicApp::draw()
{
	gl::clear(Color(0.02f, 0.02f, 0.1f));

	mShader->uniform("uLightPosition", mLightWorldPosition);
	mShader->uniform("uSkyDirection", vec4(0, 1, 0, 0));

	drawScene();
}

void prepareSettings(App::Settings *settings)
{
	settings->setTitle("Cinder VR Basic");
	settings->setWindowSize(1920 / 2, 1080 / 2);
}

CINDER_APP(BasicApp, RendererGl(RendererGl::Options().msaa(0)), prepareSettings)