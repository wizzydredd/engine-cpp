#include "app.h"

#include "types.h"
#include "input.h"

extern "C" {
	#include "../gfx/glad/glad.h"
}

NS_BEGIN

#if defined(ENG_DEBUG) && defined(GL_ARB_debug_output)

static void APIENTRY eng_gl_debug_cb(
		GLenum source, GLenum type, GLuint id,
		GLenum severity, GLsizei length,
		const GLchar* msg, const void* ud
) {
	String src = "";
	switch (source) {
		case GL_DEBUG_SOURCE_API_ARB: src = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB: src = "WINDOW SYSTEM"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: src = "SHADER COMPILER"; break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB: src = "APPLICATION"; break;
		default: src = "OTHER"; break;
	}

	String typ = "";
	switch (type) {
		case GL_DEBUG_TYPE_ERROR_ARB: typ = "ERROR"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: typ = "DEPRECATED"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB: typ = "U.B."; break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB: typ = "PERFORMANCE"; break;
		default: src = "OTHER"; break;
	}

	LogLevel lvl = LogLevel::Info;
	switch (severity) {
		case GL_DEBUG_SEVERITY_LOW_ARB: lvl = LogLevel::Warning; break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB: lvl = LogLevel::Error; break;
		case GL_DEBUG_SEVERITY_HIGH_ARB: lvl = LogLevel::Fatal; break;
		default: break;
	}

	Print(lvl, "OpenGL(", src, " [", typ, "]): ", msg);
}
#endif

Application::Application(IApplicationAdapter* adapter, ApplicationConfig config) {
	if (adapter == nullptr) {
		LogFatal("Please provide an IApplicationAdapter.");
		return;
	}

	m_applicationAdapter = uptr<IApplicationAdapter>(mov(adapter));
	m_config = mov(config);

	MessageSystem::ston().subscribe(this);
}

void Application::run() {
	if (SDL_Init(SDL_INIT_EVERYTHING) > 0) {
		LogFatal("Could not initialize SDL. ", SDL_GetError());
		return;
	}

	m_window = SDL_CreateWindow(
		m_config.title.c_str(),
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		m_config.width,
		m_config.height,
		SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL
	);

	if (m_window == nullptr) {
		SDL_Quit();
		LogFatal("Failed to create a window. ", SDL_GetError());
		return;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	
#ifdef ENG_DEBUG
	int contextFlags = 0;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &contextFlags);
	contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
#endif

	m_context = SDL_GL_CreateContext(m_window);

	if (m_context == nullptr) {
		SDL_Quit();
		LogFatal("Failed to create a context. ", SDL_GetError());
		return;
	}

	if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
		SDL_Quit();
		LogFatal("Could not load OpenGL extensions.");
		return;
	}

#if defined(ENG_DEBUG) && defined(GL_ARB_debug_output)
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

	glDebugMessageControlARB(
		GL_DEBUG_SOURCE_API_ARB,
		GL_DEBUG_TYPE_OTHER_ARB,
		GL_DEBUG_SEVERITY_LOW_ARB,
		0,
		NULL,
		GL_FALSE
	);

//	glDebugMessageCallbackARB((GLDEBUGPROCARB) eng_gl_debug_cb, NULL);
#endif

	if (GLVersion.major < 3) {
		SDL_Quit();
		LogFatal("Your GPU doesn't seem to support OpenGL 3.3 Core.");
		return;
	}

	LogInfo("OpenGL ", glGetString(GL_VERSION), ", GLSL ", glGetString(GL_SHADING_LANGUAGE_VERSION));

	m_running = true;
	LogInfo("Application Started...");
	eng_mainloop();
	LogInfo("Application Finished.");
}

void Application::processMessage(const Message& msg) {
	if (msg.type == "app_quit") {
		m_running = false;
	}
}

void Application::eng_mainloop() {
	const double timeStep = 1.0 / double(m_config.frameCap);
	double startTime = Util::getTime();
	double accum = 0.0;

	m_applicationAdapter->init();

	while (m_running) {
		bool canRender = false;
		double currentTime = Util::getTime();
		double delta = currentTime - startTime;
		startTime = currentTime;

		accum += delta;

		while (accum >= timeStep) {
			accum -= timeStep;

			Input::update();

			m_applicationAdapter->update(float(timeStep));
			canRender = true;
		}

		if (canRender) {
			m_applicationAdapter->render();
			SDL_GL_SwapWindow(m_window);
		}

		if (Input::isCloseRequested()) {
			m_running = false;
		}
	}

}

NS_END