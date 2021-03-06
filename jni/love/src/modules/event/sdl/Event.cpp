/**
 * Copyright (c) 2006-2014 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "Event.h"

#include "keyboard/Keyboard.h"
#include "mouse/Mouse.h"
#include "joystick/JoystickModule.h"
#include "joystick/sdl/Joystick.h"
#include "graphics/Graphics.h"
#include "window/Window.h"
#include "common/Exception.h"
#include "audio/Audio.h"

#include <cmath>

namespace love
{
namespace event
{
namespace sdl
{

// SDL reports mouse coordinates in the window coordinate system in OS X, but
// we want them in pixel coordinates (may be different with high-DPI enabled.)
static void windowToPixelCoords(int *x, int *y)
{
#ifndef LOVE_ANDROID
	double scale = 1.0;

	window::Window *window = (window::Window *) Module::findInstance("love.window.");
	if (window != nullptr)
		scale = window->getPixelScale();

	if (x != nullptr)
		*x = int(double(*x) * scale);

	if (y != nullptr)
		*y = int(double(*y) * scale);
#endif
}


const char *Event::getName() const
{
	return "love.event.sdl";
}

Event::Event()
{
	if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0)
		throw love::Exception("%s", SDL_GetError());
}

Event::~Event()
{
	SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

void Event::pump()
{
	SDL_PumpEvents();

	static SDL_Event e;

	Message *msg;

	while (SDL_PollEvent(&e))
	{
		msg = convert(e);
		if (msg)
		{
			push(msg);
			msg->release();
		}
	}
}

Message *Event::wait()
{
	static SDL_Event e;
	bool ok = (SDL_WaitEvent(&e) == 1);
	if (!ok)
		return NULL;
	return convert(e);
}

void Event::clear()
{
	static SDL_Event e;

	while (SDL_PollEvent(&e))
	{
		// Do nothing with 'e' ...
	}

	love::event::Event::clear();
}

Message *Event::convert(const SDL_Event &e) const
{
	Message *msg = nullptr;

	std::vector<Variant *> vargs;
	vargs.reserve(4);

	love::keyboard::Keyboard *kb = nullptr;

	love::keyboard::Keyboard::Key key;
	love::mouse::Mouse::Button button;
	const char *txt;
	std::map<SDL_Keycode, love::keyboard::Keyboard::Key>::const_iterator keyit;

	switch (e.type)
	{
	case SDL_KEYDOWN:
		if (e.key.repeat)
		{
			kb = (love::keyboard::Keyboard *) Module::findInstance("love.keyboard.");
			if (kb && !kb->hasKeyRepeat())
				break;
		}

		keyit = keys.find(e.key.keysym.sym);
		if (keyit != keys.end())
			key = keyit->second;
		else
			key = love::keyboard::Keyboard::KEY_UNKNOWN;

		if (!love::keyboard::Keyboard::getConstant(key, txt))
			txt = "unknown";

		vargs.push_back(new Variant(txt, strlen(txt)));
		vargs.push_back(new Variant(e.key.repeat != 0));
		msg = new Message("keypressed", vargs);
		break;
	case SDL_KEYUP:
		keyit = keys.find(e.key.keysym.sym);
		if (keyit != keys.end())
			key = keyit->second;
		else
			key = love::keyboard::Keyboard::KEY_UNKNOWN;

		if (!love::keyboard::Keyboard::getConstant(key, txt))
			txt = "unknown";

		vargs.push_back(new Variant(txt, strlen(txt)));
		msg = new Message("keyreleased", vargs);
		break;
	case SDL_TEXTINPUT:
		txt = e.text.text;
		vargs.push_back(new Variant(txt, strlen(txt)));
		msg = new Message("textinput", vargs);
		break;
	case SDL_TEXTEDITING:
		txt = e.edit.text;
		vargs.push_back(new Variant(txt, strlen(txt)));
		vargs.push_back(new Variant((double) e.edit.start));
		vargs.push_back(new Variant((double) e.edit.length));
		msg = new Message("textedit", vargs);
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (buttons.find(e.button.button, button) && mouse::Mouse::getConstant(button, txt))
		{
			int x = e.button.x;
			int y = e.button.y;
			windowToPixelCoords(&x, &y);
			vargs.push_back(new Variant((double) x));
			vargs.push_back(new Variant((double) y));
			vargs.push_back(new Variant(txt, strlen(txt)));
			vargs.push_back(new Variant(e.button.which == SDL_TOUCH_MOUSEID));
			msg = new Message((e.type == SDL_MOUSEBUTTONDOWN) ?
							  "mousepressed" : "mousereleased",
							  vargs);
		}
		break;
	case SDL_MOUSEWHEEL:
		if (e.wheel.y != 0)
		{
			button = (e.wheel.y > 0) ? mouse::Mouse::BUTTON_WHEELUP : mouse::Mouse::BUTTON_WHEELDOWN;
			if (!love::mouse::Mouse::getConstant(button, txt))
				break;

			int mx, my;
			SDL_GetMouseState(&mx, &my);
			windowToPixelCoords(&mx, &my);

			vargs.push_back(new Variant((double) mx));
			vargs.push_back(new Variant((double) my));
			vargs.push_back(new Variant(txt, strlen(txt)));
			vargs.push_back(new Variant(false));
			msg = new Message("mousepressed", vargs);
		}
		break;
	case SDL_FINGERDOWN:
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
		vargs.push_back(new Variant((double) e.tfinger.fingerId));
		vargs.push_back(new Variant((double) e.tfinger.x));
		vargs.push_back(new Variant((double) e.tfinger.y));
		vargs.push_back(new Variant((double) e.tfinger.pressure));
		if (e.type == SDL_FINGERDOWN)
			txt = "touchpressed";
		else if (e.type == SDL_FINGERUP)
			txt = "touchreleased";
		else
			txt = "touchmoved";
		msg = new Message(txt, vargs);
		break;
	case SDL_MULTIGESTURE:
		vargs.push_back(new Variant((double) e.mgesture.x));
		vargs.push_back(new Variant((double) e.mgesture.y));
		vargs.push_back(new Variant((double) e.mgesture.dTheta));
		vargs.push_back(new Variant((double) e.mgesture.dDist));
		vargs.push_back(new Variant((double) e.mgesture.numFingers));
		msg = new Message("touchgestured", vargs);
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
	case SDL_JOYAXISMOTION:
	case SDL_JOYBALLMOTION:
	case SDL_JOYHATMOTION:
	case SDL_JOYDEVICEADDED:
	case SDL_JOYDEVICEREMOVED:
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	case SDL_CONTROLLERAXISMOTION:
		msg = convertJoystickEvent(e);
		break;
	case SDL_WINDOWEVENT:
		msg = convertWindowEvent(e);
		break;
	case SDL_DROPFILE:
		SDL_free(e.drop.file);
		break;
	case SDL_QUIT:
		msg = new Message("quit");
		break;
	default:
		break;
	}

	for (auto it = vargs.begin(); it != vargs.end(); ++it)
	{
		if ((*it) != nullptr)
			(*it)->release();
	}

	return msg;
}

Message *Event::convertJoystickEvent(const SDL_Event &e) const
{
	joystick::JoystickModule *joymodule = (joystick::JoystickModule *) Module::findInstance("love.joystick.");
	if (!joymodule)
		return nullptr;

	Message *msg = nullptr;

	std::vector<Variant *> vargs;
	vargs.reserve(4);

	Proxy proxy;
	love::joystick::Joystick::Hat hat;
	love::joystick::Joystick::GamepadButton padbutton;
	love::joystick::Joystick::GamepadAxis padaxis;
	const char *txt;

	switch (e.type)
	{
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		proxy.flags = JOYSTICK_JOYSTICK_T;
		proxy.data = joymodule->getJoystickFromID(e.jbutton.which);
		if (!proxy.data)
			break;

		vargs.push_back(new Variant(JOYSTICK_JOYSTICK_ID, (void *) &proxy));
		vargs.push_back(new Variant((double)(e.jbutton.button+1)));
		msg = new Message((e.type == SDL_JOYBUTTONDOWN) ?
						  "joystickpressed" : "joystickreleased",
						  vargs);
		break;
	case SDL_JOYAXISMOTION:
		{
			proxy.flags = JOYSTICK_JOYSTICK_T;
			proxy.data = joymodule->getJoystickFromID(e.jaxis.which);
			if (!proxy.data)
				break;

			vargs.push_back(new Variant(JOYSTICK_JOYSTICK_ID, (void *) &proxy));
			vargs.push_back(new Variant((double)(e.jaxis.axis+1)));
			float value = e.jaxis.value / 32768.0f;
			if (fabsf(value) < 0.001f) value = 0.0f;
			if (value < -0.99f) value = -1.0f;
			if (value > 0.99f) value = 1.0f;
			vargs.push_back(new Variant((double) value));
			msg = new Message("joystickaxis", vargs);
		}
		break;
	case SDL_JOYHATMOTION:
		if (!joystick::sdl::Joystick::getConstant(e.jhat.value, hat) || !joystick::Joystick::getConstant(hat, txt))
			break;

		proxy.flags = JOYSTICK_JOYSTICK_T;
		proxy.data = joymodule->getJoystickFromID(e.jhat.which);
		if (!proxy.data)
			break;

		vargs.push_back(new Variant(JOYSTICK_JOYSTICK_ID, (void *) &proxy));
		vargs.push_back(new Variant((double)(e.jhat.hat+1)));
		vargs.push_back(new Variant(txt, strlen(txt)));
		msg = new Message("joystickhat", vargs);
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		if (!joystick::sdl::Joystick::getConstant((SDL_GameControllerButton) e.cbutton.button, padbutton))
			break;

		if (!joystick::Joystick::getConstant(padbutton, txt))
			break;

		proxy.flags = JOYSTICK_JOYSTICK_T;
		proxy.data = joymodule->getJoystickFromID(e.cbutton.which);
		if (!proxy.data)
			break;

		vargs.push_back(new Variant(JOYSTICK_JOYSTICK_ID, (void *) &proxy));
		vargs.push_back(new Variant(txt, strlen(txt)));
		msg = new Message(e.type == SDL_CONTROLLERBUTTONDOWN ?
						  "gamepadpressed" : "gamepadreleased", vargs);
		break;
	case SDL_CONTROLLERAXISMOTION:
		if (joystick::sdl::Joystick::getConstant((SDL_GameControllerAxis) e.caxis.axis, padaxis))
		{
			if (!joystick::Joystick::getConstant(padaxis, txt))
				break;

			proxy.flags = JOYSTICK_JOYSTICK_T;
			proxy.data = joymodule->getJoystickFromID(e.caxis.which);
			if (!proxy.data)
				break;

			vargs.push_back(new Variant(JOYSTICK_JOYSTICK_ID, (void *) &proxy));

			vargs.push_back(new Variant(txt, strlen(txt)));
			float value = e.jaxis.value / 32768.0f;
			if (fabsf(value) < 0.001f) value = 0.0f;
			if (value < -0.99f) value = -1.0f;
			if (value > 0.99f) value = 1.0f;
			vargs.push_back(new Variant((double) value));
			msg = new Message("gamepadaxis", vargs);
		}
		break;
	case SDL_JOYDEVICEADDED:
		// jdevice.which is the joystick device index.
		proxy.data = joymodule->addJoystick(e.jdevice.which);
		proxy.flags = JOYSTICK_JOYSTICK_T;
		if (proxy.data)
		{
			vargs.push_back(new Variant(JOYSTICK_JOYSTICK_ID, (void *) &proxy));
			msg = new Message("joystickadded", vargs);
		}
		break;
	case SDL_JOYDEVICEREMOVED:
		// jdevice.which is the joystick instance ID now.
		proxy.data = joymodule->getJoystickFromID(e.jdevice.which);
		proxy.flags = JOYSTICK_JOYSTICK_T;
		if (proxy.data)
		{
			joymodule->removeJoystick((joystick::Joystick *) proxy.data);
			vargs.push_back(new Variant(JOYSTICK_JOYSTICK_ID, (void *) &proxy));
			msg = new Message("joystickremoved", vargs);
		}
		break;
	default:
		break;
	}

	for (auto it = vargs.begin(); it != vargs.end(); ++it)
	{
		if ((*it) != nullptr)
			(*it)->release();
	}

	return msg;
}

Message *Event::convertWindowEvent(const SDL_Event &e) const
{
	Message *msg = nullptr;

	std::vector<Variant *> vargs;
	vargs.reserve(4);

	window::Window *win = nullptr;

	if (e.type != SDL_WINDOWEVENT)
		return nullptr;

	switch (e.window.event)
	{
	case SDL_WINDOWEVENT_FOCUS_GAINED:
	case SDL_WINDOWEVENT_FOCUS_LOST:
		// Users won't expect the screensaver to activate if a game is in
		// focus. Also, joystick input may not delay the screensaver timer.
		if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
			SDL_DisableScreenSaver();
		else
			SDL_EnableScreenSaver();
		vargs.push_back(new Variant(e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED));
		msg = new Message("focus", vargs);
		break;
	case SDL_WINDOWEVENT_ENTER:
	case SDL_WINDOWEVENT_LEAVE:
		vargs.push_back(new Variant(e.window.event == SDL_WINDOWEVENT_ENTER));
		msg = new Message("mousefocus", vargs);
		break;
	case SDL_WINDOWEVENT_SHOWN:
	case SDL_WINDOWEVENT_HIDDEN:
		vargs.push_back(new Variant(e.window.event == SDL_WINDOWEVENT_SHOWN));
		msg = new Message("visible", vargs);
		break;
	case SDL_WINDOWEVENT_RESIZED:
		win = (window::Window *) Module::findInstance("love.window.");
		if (win)
		{
			int px_w = e.window.data1;
			int px_h = e.window.data2;

#if SDL_VERSION_ATLEAST(2,0,1)
			SDL_Window *sdlwin = SDL_GetWindowFromID(e.window.windowID);
			if (sdlwin)
				SDL_GL_GetDrawableSize(sdlwin, &px_w, &px_h);
#endif

			win->onWindowResize(e.window.data1, e.window.data2);

			graphics::Graphics *gfx = (graphics::Graphics *) Module::findInstance("love.graphics.");
			if (gfx)
				gfx->setViewportSize(px_w, px_h);

			vargs.push_back(new Variant((double) px_w));
			vargs.push_back(new Variant((double) px_h));
			vargs.push_back(new Variant((double) e.window.data1));
			vargs.push_back(new Variant((double) e.window.data2));
			msg = new Message("resize", vargs);
		}
		break;
#ifdef LOVE_ANDROID
		case SDL_WINDOWEVENT_MINIMIZED:
		{
			audio::Audio *audio = (audio::Audio *) Module::findInstance("love.audio.");
			if (audio)
				audio->pause();
		}
		break;
		case SDL_WINDOWEVENT_RESTORED:
		{
			audio::Audio *audio = (audio::Audio *) Module::findInstance("love.audio.");
			if (audio)
				audio->resume();
		}
		break;
#endif
	}

	for (auto it = vargs.begin(); it != vargs.end(); ++it)
	{
		if ((*it) != nullptr)
			(*it)->release();
	}

	return msg;
}

std::map<SDL_Keycode, love::keyboard::Keyboard::Key> Event::createKeyMap()
{
	using love::keyboard::Keyboard;

	std::map<SDL_Keycode, Keyboard::Key> k;

	k[SDLK_UNKNOWN] = Keyboard::KEY_UNKNOWN;

	k[SDLK_RETURN] = Keyboard::KEY_RETURN;
	k[SDLK_ESCAPE] = Keyboard::KEY_ESCAPE;
	k[SDLK_BACKSPACE] = Keyboard::KEY_BACKSPACE;
	k[SDLK_TAB] = Keyboard::KEY_TAB;
	k[SDLK_SPACE] = Keyboard::KEY_SPACE;
	k[SDLK_EXCLAIM] = Keyboard::KEY_EXCLAIM;
	k[SDLK_QUOTEDBL] = Keyboard::KEY_QUOTEDBL;
	k[SDLK_HASH] = Keyboard::KEY_HASH;
	k[SDLK_DOLLAR] = Keyboard::KEY_DOLLAR;
	k[SDLK_AMPERSAND] = Keyboard::KEY_AMPERSAND;
	k[SDLK_QUOTE] = Keyboard::KEY_QUOTE;
	k[SDLK_LEFTPAREN] = Keyboard::KEY_LEFTPAREN;
	k[SDLK_RIGHTPAREN] = Keyboard::KEY_RIGHTPAREN;
	k[SDLK_ASTERISK] = Keyboard::KEY_ASTERISK;
	k[SDLK_PLUS] = Keyboard::KEY_PLUS;
	k[SDLK_COMMA] = Keyboard::KEY_COMMA;
	k[SDLK_MINUS] = Keyboard::KEY_MINUS;
	k[SDLK_PERIOD] = Keyboard::KEY_PERIOD;
	k[SDLK_SLASH] = Keyboard::KEY_SLASH;
	k[SDLK_0] = Keyboard::KEY_0;
	k[SDLK_1] = Keyboard::KEY_1;
	k[SDLK_2] = Keyboard::KEY_2;
	k[SDLK_3] = Keyboard::KEY_3;
	k[SDLK_4] = Keyboard::KEY_4;
	k[SDLK_5] = Keyboard::KEY_5;
	k[SDLK_6] = Keyboard::KEY_6;
	k[SDLK_7] = Keyboard::KEY_7;
	k[SDLK_8] = Keyboard::KEY_8;
	k[SDLK_9] = Keyboard::KEY_9;
	k[SDLK_COLON] = Keyboard::KEY_COLON;
	k[SDLK_SEMICOLON] = Keyboard::KEY_SEMICOLON;
	k[SDLK_LESS] = Keyboard::KEY_LESS;
	k[SDLK_EQUALS] = Keyboard::KEY_EQUALS;
	k[SDLK_GREATER] = Keyboard::KEY_GREATER;
	k[SDLK_QUESTION] = Keyboard::KEY_QUESTION;
	k[SDLK_AT] = Keyboard::KEY_AT;

	k[SDLK_LEFTBRACKET] = Keyboard::KEY_LEFTBRACKET;
	k[SDLK_BACKSLASH] = Keyboard::KEY_BACKSLASH;
	k[SDLK_RIGHTBRACKET] = Keyboard::KEY_RIGHTBRACKET;
	k[SDLK_CARET] = Keyboard::KEY_CARET;
	k[SDLK_UNDERSCORE] = Keyboard::KEY_UNDERSCORE;
	k[SDLK_BACKQUOTE] = Keyboard::KEY_BACKQUOTE;
	k[SDLK_a] = Keyboard::KEY_A;
	k[SDLK_b] = Keyboard::KEY_B;
	k[SDLK_c] = Keyboard::KEY_C;
	k[SDLK_d] = Keyboard::KEY_D;
	k[SDLK_e] = Keyboard::KEY_E;
	k[SDLK_f] = Keyboard::KEY_F;
	k[SDLK_g] = Keyboard::KEY_G;
	k[SDLK_h] = Keyboard::KEY_H;
	k[SDLK_i] = Keyboard::KEY_I;
	k[SDLK_j] = Keyboard::KEY_J;
	k[SDLK_k] = Keyboard::KEY_K;
	k[SDLK_l] = Keyboard::KEY_L;
	k[SDLK_m] = Keyboard::KEY_M;
	k[SDLK_n] = Keyboard::KEY_N;
	k[SDLK_o] = Keyboard::KEY_O;
	k[SDLK_p] = Keyboard::KEY_P;
	k[SDLK_q] = Keyboard::KEY_Q;
	k[SDLK_r] = Keyboard::KEY_R;
	k[SDLK_s] = Keyboard::KEY_S;
	k[SDLK_t] = Keyboard::KEY_T;
	k[SDLK_u] = Keyboard::KEY_U;
	k[SDLK_v] = Keyboard::KEY_V;
	k[SDLK_w] = Keyboard::KEY_W;
	k[SDLK_x] = Keyboard::KEY_X;
	k[SDLK_y] = Keyboard::KEY_Y;
	k[SDLK_z] = Keyboard::KEY_Z;

	k[SDLK_CAPSLOCK] = Keyboard::KEY_CAPSLOCK;

	k[SDLK_F1] = Keyboard::KEY_F1;
	k[SDLK_F2] = Keyboard::KEY_F2;
	k[SDLK_F3] = Keyboard::KEY_F3;
	k[SDLK_F4] = Keyboard::KEY_F4;
	k[SDLK_F5] = Keyboard::KEY_F5;
	k[SDLK_F6] = Keyboard::KEY_F6;
	k[SDLK_F7] = Keyboard::KEY_F7;
	k[SDLK_F8] = Keyboard::KEY_F8;
	k[SDLK_F9] = Keyboard::KEY_F9;
	k[SDLK_F10] = Keyboard::KEY_F10;
	k[SDLK_F11] = Keyboard::KEY_F11;
	k[SDLK_F12] = Keyboard::KEY_F12;

	k[SDLK_PRINTSCREEN] = Keyboard::KEY_PRINTSCREEN;
	k[SDLK_SCROLLLOCK] = Keyboard::KEY_SCROLLLOCK;
	k[SDLK_PAUSE] = Keyboard::KEY_PAUSE;
	k[SDLK_INSERT] = Keyboard::KEY_INSERT;
	k[SDLK_HOME] = Keyboard::KEY_HOME;
	k[SDLK_PAGEUP] = Keyboard::KEY_PAGEUP;
	k[SDLK_DELETE] = Keyboard::KEY_DELETE;
	k[SDLK_END] = Keyboard::KEY_END;
	k[SDLK_PAGEDOWN] = Keyboard::KEY_PAGEDOWN;
	k[SDLK_RIGHT] = Keyboard::KEY_RIGHT;
	k[SDLK_LEFT] = Keyboard::KEY_LEFT;
	k[SDLK_DOWN] = Keyboard::KEY_DOWN;
	k[SDLK_UP] = Keyboard::KEY_UP;

	k[SDLK_NUMLOCKCLEAR] = Keyboard::KEY_NUMLOCKCLEAR;
	k[SDLK_KP_DIVIDE] = Keyboard::KEY_KP_DIVIDE;
	k[SDLK_KP_MULTIPLY] = Keyboard::KEY_KP_MULTIPLY;
	k[SDLK_KP_MINUS] = Keyboard::KEY_KP_MINUS;
	k[SDLK_KP_PLUS] = Keyboard::KEY_KP_PLUS;
	k[SDLK_KP_ENTER] = Keyboard::KEY_KP_ENTER;
	k[SDLK_KP_0] = Keyboard::KEY_KP_0;
	k[SDLK_KP_1] = Keyboard::KEY_KP_1;
	k[SDLK_KP_2] = Keyboard::KEY_KP_2;
	k[SDLK_KP_3] = Keyboard::KEY_KP_3;
	k[SDLK_KP_4] = Keyboard::KEY_KP_4;
	k[SDLK_KP_5] = Keyboard::KEY_KP_5;
	k[SDLK_KP_6] = Keyboard::KEY_KP_6;
	k[SDLK_KP_7] = Keyboard::KEY_KP_7;
	k[SDLK_KP_8] = Keyboard::KEY_KP_8;
	k[SDLK_KP_9] = Keyboard::KEY_KP_9;
	k[SDLK_KP_PERIOD] = Keyboard::KEY_KP_PERIOD;
	k[SDLK_KP_COMMA] = Keyboard::KEY_KP_COMMA;
	k[SDLK_KP_EQUALS] = Keyboard::KEY_KP_EQUALS;

	k[SDLK_APPLICATION] = Keyboard::KEY_APPLICATION;
	k[SDLK_POWER] = Keyboard::KEY_POWER;
	k[SDLK_F13] = Keyboard::KEY_F13;
	k[SDLK_F14] = Keyboard::KEY_F14;
	k[SDLK_F15] = Keyboard::KEY_F15;
	k[SDLK_F16] = Keyboard::KEY_F16;
	k[SDLK_F17] = Keyboard::KEY_F17;
	k[SDLK_F18] = Keyboard::KEY_F18;
	k[SDLK_F19] = Keyboard::KEY_F19;
	k[SDLK_F20] = Keyboard::KEY_F20;
	k[SDLK_F21] = Keyboard::KEY_F21;
	k[SDLK_F22] = Keyboard::KEY_F22;
	k[SDLK_F23] = Keyboard::KEY_F23;
	k[SDLK_F24] = Keyboard::KEY_F24;
	k[SDLK_EXECUTE] = Keyboard::KEY_EXECUTE;
	k[SDLK_HELP] = Keyboard::KEY_HELP;
	k[SDLK_MENU] = Keyboard::KEY_MENU;
	k[SDLK_AC_SEARCH] = Keyboard::KEY_SEARCH;
	k[SDLK_SELECT] = Keyboard::KEY_SELECT;
	k[SDLK_STOP] = Keyboard::KEY_STOP;
	k[SDLK_AGAIN] = Keyboard::KEY_AGAIN;
	k[SDLK_UNDO] = Keyboard::KEY_UNDO;
	k[SDLK_CUT] = Keyboard::KEY_CUT;
	k[SDLK_COPY] = Keyboard::KEY_COPY;
	k[SDLK_PASTE] = Keyboard::KEY_PASTE;
	k[SDLK_FIND] = Keyboard::KEY_FIND;
	k[SDLK_MUTE] = Keyboard::KEY_MUTE;
	k[SDLK_VOLUMEUP] = Keyboard::KEY_VOLUMEUP;
	k[SDLK_VOLUMEDOWN] = Keyboard::KEY_VOLUMEDOWN;

	k[SDLK_ALTERASE] = Keyboard::KEY_ALTERASE;
	k[SDLK_SYSREQ] = Keyboard::KEY_SYSREQ;
	k[SDLK_CANCEL] = Keyboard::KEY_CANCEL;
	k[SDLK_CLEAR] = Keyboard::KEY_CLEAR;
	k[SDLK_PRIOR] = Keyboard::KEY_PRIOR;
	k[SDLK_RETURN2] = Keyboard::KEY_RETURN2;
	k[SDLK_SEPARATOR] = Keyboard::KEY_SEPARATOR;
	k[SDLK_OUT] = Keyboard::KEY_OUT;
	k[SDLK_OPER] = Keyboard::KEY_OPER;
	k[SDLK_CLEARAGAIN] = Keyboard::KEY_CLEARAGAIN;

	k[SDLK_THOUSANDSSEPARATOR] = Keyboard::KEY_THOUSANDSSEPARATOR;
	k[SDLK_DECIMALSEPARATOR] = Keyboard::KEY_DECIMALSEPARATOR;
	k[SDLK_CURRENCYUNIT] = Keyboard::KEY_CURRENCYUNIT;
	k[SDLK_CURRENCYSUBUNIT] = Keyboard::KEY_CURRENCYSUBUNIT;

	k[SDLK_LCTRL] = Keyboard::KEY_LCTRL;
	k[SDLK_LSHIFT] = Keyboard::KEY_LSHIFT;
	k[SDLK_LALT] = Keyboard::KEY_LALT;
	k[SDLK_LGUI] = Keyboard::KEY_LGUI;
	k[SDLK_RCTRL] = Keyboard::KEY_RCTRL;
	k[SDLK_RSHIFT] = Keyboard::KEY_RSHIFT;
	k[SDLK_RALT] = Keyboard::KEY_RALT;
	k[SDLK_RGUI] = Keyboard::KEY_RGUI;

	k[SDLK_MODE] = Keyboard::KEY_MODE;

	k[SDLK_AUDIONEXT] = Keyboard::KEY_AUDIONEXT;
	k[SDLK_AUDIOPREV] = Keyboard::KEY_AUDIOPREV;
	k[SDLK_AUDIOSTOP] = Keyboard::KEY_AUDIOSTOP;
	k[SDLK_AUDIOPLAY] = Keyboard::KEY_AUDIOPLAY;
	k[SDLK_AUDIOMUTE] = Keyboard::KEY_AUDIOMUTE;
	k[SDLK_MEDIASELECT] = Keyboard::KEY_MEDIASELECT;

	k[SDLK_BRIGHTNESSDOWN] = Keyboard::KEY_BRIGHTNESSDOWN;
	k[SDLK_BRIGHTNESSUP] = Keyboard::KEY_BRIGHTNESSUP;
	k[SDLK_DISPLAYSWITCH] = Keyboard::KEY_DISPLAYSWITCH;
	k[SDLK_KBDILLUMTOGGLE] = Keyboard::KEY_KBDILLUMTOGGLE;
	k[SDLK_KBDILLUMDOWN] = Keyboard::KEY_KBDILLUMDOWN;
	k[SDLK_KBDILLUMUP] = Keyboard::KEY_KBDILLUMUP;
	k[SDLK_EJECT] = Keyboard::KEY_EJECT;
	k[SDLK_SLEEP] = Keyboard::KEY_SLEEP;

#ifdef LOVE_ANDROID
	k[SDLK_AC_BACK] = Keyboard::KEY_ESCAPE;
#endif

	return k;
}

std::map<SDL_Keycode, love::keyboard::Keyboard::Key> Event::keys = Event::createKeyMap();

EnumMap<love::mouse::Mouse::Button, Uint8, love::mouse::Mouse::BUTTON_MAX_ENUM>::Entry Event::buttonEntries[] =
{
	{ love::mouse::Mouse::BUTTON_LEFT, SDL_BUTTON_LEFT},
	{ love::mouse::Mouse::BUTTON_MIDDLE, SDL_BUTTON_MIDDLE},
	{ love::mouse::Mouse::BUTTON_RIGHT, SDL_BUTTON_RIGHT},
	{ love::mouse::Mouse::BUTTON_X1, SDL_BUTTON_X1},
	{ love::mouse::Mouse::BUTTON_X2, SDL_BUTTON_X2},
};

EnumMap<love::mouse::Mouse::Button, Uint8, love::mouse::Mouse::BUTTON_MAX_ENUM> Event::buttons(Event::buttonEntries, sizeof(Event::buttonEntries));

} // sdl
} // event
} // love
