/*
 *	Copyright 2016, Maxime Journaux <journaux.maxime@gmail.com>
 * 	This work is free. You can redistribute it and/or modify it under the
 *	terms of the Do What The Fuck You Want To Public License, Version 2,
 *	as published by Sam Hocevar. 
 *	See http://www.wtfpl.net for more details.
 */

#include <v8.h>
#include <node.h>

#include <stdlib.h>
#include <unistd.h>
#include <string> 
#include <algorithm>

#include <led-matrix.h>
#include <ledmatrix.h>
#include <graphics.h>

#include <iostream>

using namespace v8;
using namespace node;
using namespace rgb_matrix;
using rgb_matrix::GPIO;

Nan::Persistent<v8::Function> LedMatrix::constructor;
std::map<std::string, rgb_matrix::Font> LedMatrix::fontMap;

LedMatrix::LedMatrix (int rows, int cols , int parallel_displays, int chained_displays, int brightness, const char* mapping, const char* rgbseq, std::vector<std::string> flags) 
{

	//dump out flags into a vector of char* <CRRINNGGEEEEEE>
	std::vector<char*> c_strs;
	c_strs.push_back("bin");
	for(auto &f : flags) //FIXED THIS BEING VALUE LOOP INSTEAD OF REFERENCE LOOP AAAAAAAAAAAAAHHHHHHHHHHHHHHHHHHHHHHHH
	{
		c_strs.push_back(&f[0]); //every known version of std::string uses continuos memory so this is safe~ 
		std::cout << &f[0] << std::endl;
	}

	int num = c_strs.size();
    char** d = c_strs.data();

	RGBMatrix::Options defaults; 
    rgb_matrix::RuntimeOptions runtime;


	defaults.rows = rows;
	defaults.cols = cols; 
	defaults.chain_length = chained_displays;
	defaults.parallel = parallel_displays; 
	defaults.brightness = brightness;
	defaults.hardware_mapping = mapping;
	defaults.led_rgb_sequence = rgbseq;

	std::cout << rgb_matrix::ParseOptionsFromFlags(&num, &d, &defaults, &runtime, true) << std::endl;
	//temp debug output
	//parse extra settings flags for POWER_USERS (TM)

	assert(io.Init());
	//matrix = CreateMatrixFromOptions(defaults, runtime);
	matrix = new RGBMatrix(&io, defaults);	
	matrix->set_luminance_correct(true);

	canvas = matrix->CreateFrameCanvas();
	image = NULL;
}

LedMatrix::~LedMatrix (void) 
{
	delete image;
	delete matrix;
}

void LedMatrix::Init (v8::Local<v8::Object> exports) 
{
	Nan::HandleScope scope;
	
	v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
	
	tpl->SetClassName(Nan::New("LedMatrix").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(tpl, "getWidth", GetWidth);
	Nan::SetPrototypeMethod(tpl, "getHeight", GetHeight);
	Nan::SetPrototypeMethod(tpl, "setPixel", SetPixel);
	Nan::SetPrototypeMethod(tpl, "clear", Clear);
	Nan::SetPrototypeMethod(tpl, "fill", Fill);
	Nan::SetPrototypeMethod(tpl, "setImageBuffer", SetImageBuffer);
	Nan::SetPrototypeMethod(tpl, "draw", Draw);
	Nan::SetPrototypeMethod(tpl, "scroll", Scroll);
	Nan::SetPrototypeMethod(tpl, "update", Update);
	Nan::SetPrototypeMethod(tpl, "drawText", DrawText);
	Nan::SetPrototypeMethod(tpl, "drawCircle", DrawCircle);
	Nan::SetPrototypeMethod(tpl, "drawLine", DrawLine);
	Nan::SetPrototypeMethod(tpl, "brightness", Brightness);
	
	constructor.Reset(tpl->GetFunction());

	exports->Set(Nan::New("LedMatrix").ToLocalChecked(), tpl->GetFunction());
}

int LedMatrix::GetWidth	(void) 
{
	return canvas->width();
}

int LedMatrix::GetHeight (void) 
{
	return canvas->height();
}

void LedMatrix::SetPixel (int x, int y, uint8_t r, uint8_t g, uint8_t b) 
{
	canvas->SetPixel(x, y, r, g, b);
}

void LedMatrix::Clear (void) 
{
	canvas->Clear();
}

void LedMatrix::Clear (int x, int y, int w, int h) 
{
	for(int j=y; j<y+h; j++) 
	{
		for(int i=x; i<x+w; i++) 
		{
			canvas->SetPixel(i, j, 0, 0, 0);
		}
	}
}

void LedMatrix::Fill (uint8_t r, uint8_t g, uint8_t b) 
{
	canvas->Fill(r, g, b);
}

void LedMatrix::SetImage (Image* img) 
{
	if(image && image->IsValid()) 
	{
		delete image;
		image = NULL;
	}

	image = new Image();
	image->Reset();
	image->SetWidth(img->GetWidth());
	image->SetHeight(img->GetHeight());
	int amount = image->GetWidth() * image->GetHeight();
	Pixel* p = new Pixel[amount];
	memcpy(p, img->GetPixels(), sizeof(Pixel) * amount);
	image->SetPixels(p);
}

void LedMatrix::Draw (int screenx, int screeny, int width, int height, int imgx, int imgy, bool looph, bool loopv) 
{
	int w = width;
	if(image->GetWidth() - imgx < w && !looph)
		w = image->GetWidth() - imgx;

	int h = height;
	if(image->GetHeight() - imgy < h && !loopv)
		h = image->GetHeight() - imgy;

	for(int i = 0; i < w; i++) 
	{
		int px = (i + imgx) % image->GetWidth();
		int sx = i + screenx;
		for(int j = 0; j < h; j++) 
		{
			int py = (j + imgy) % image->GetHeight();
			int sy = j + screeny;
			Pixel p = image->GetPixel(px, py);
			canvas->SetPixel(sx, sy, p.R(), p.G(), p.B());
		}
	}
}	

void LedMatrix :: Update (void)
{
	const char* data; 
	size_t len; 

	canvas->Serialize(&data, &len); 
	canvas = matrix->SwapOnVSync(canvas);
	canvas->Deserialize(data, len);
}

void LedMatrix :: DrawText (int x, int y, std::tuple<int, int, int> color, const char* text, const char* fontFile) 
{
	const std::string file = fontFile; 
	auto it = fontMap.find(file); 

	if(it == fontMap.end())
	{
		fontMap.emplace(std::piecewise_construct, std::make_tuple(file), std::make_tuple());
		it = fontMap.find(file);
		it->second.LoadFont(file.c_str());
	}
	
	Color bg(0,0,0);
	Color fg(std::get<0>(color), std::get<1>(color), std::get<2>(color));
	rgb_matrix::DrawText(canvas, it->second, x, y + it->second.baseline(), fg, text); 
}

void LedMatrix :: DrawText (const Nan::FunctionCallbackInfo<Value>& args)  
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());
	std::string text; 
	std::string font; 
	int x = 0; 
	int y = 0; 

	int r = 255; 
	int g = 255; 
	int b = 255; 

	if(args.Length() > 0 && args[0]->IsNumber())
	{
		x = args[0]->ToInteger()->Value();
	}

	if(args.Length() > 1 && args[1]->IsNumber())
	{
		y = args[1]->ToInteger()->Value();
	}

	if(args.Length() > 2 && args[2]->IsString()) 
	{
		v8::String::Utf8Value str(args[2]->ToString()); 
		text = std::string(*str); 
	}

	if(args.Length() > 3 && args[3]->IsString()) 
	{
		v8::String::Utf8Value str(args[3]->ToString()); 
		font = std::string(*str); 
	}

	if(args.Length() > 4 && args[4]->IsNumber())
	{
		r = args[4]->ToInteger()->Value();
	}

	if(args.Length() > 5 && args[5]->IsNumber())
	{
		g = args[5]->ToInteger()->Value();
	}

	if(args.Length() > 6 && args[6]->IsNumber())
	{
		b = args[6]->ToInteger()->Value();
	}
	
	return matrix->DrawText(x, y, std::make_tuple(r,g,b), text.c_str(), font.c_str());
}

void LedMatrix :: DrawCircle (int x, int y, int radius, std::tuple<int, int, int> color)
{
	Color fg( std::get<0>(color), std::get<1>(color), std::get<2>(color)); 

	rgb_matrix::DrawCircle(canvas, x, y, radius, fg);
}

void LedMatrix :: DrawCircle (const Nan::FunctionCallbackInfo<Value>& args)
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder()); 
	int x = 0; 
	int y = 0; 
	int radius = 0; 

	int r = 255; 
	int g = 255; 
	int b = 255; 

	if(args.Length() > 0 && args[0]->IsNumber())
	{
		x = args[0]->ToInteger()->Value();
	}

	if(args.Length() > 1 && args[1]->IsNumber()) 
	{
		y = args[1]->ToInteger()->Value(); 
	}

	if(args.Length() > 2 && args[2]->IsNumber())
	{
		radius = args[2]->ToInteger()->Value();
	}

	if(args.Length() > 3 && args[3]->IsNumber())
	{
		r = args[3]->ToInteger()->Value();
	}

	if(args.Length() > 4 && args[4]->IsNumber())
	{
		g = args[4]->ToInteger()->Value();
	}

	if(args.Length() > 5 && args[5]->IsNumber())
	{
		b = args[5]->ToInteger()->Value();
	}

	return matrix->DrawCircle( x, y, radius, std::make_tuple(r, g, b));
}
	
void LedMatrix :: DrawLine   (int x0, int y0, int x1, int y1, std::tuple<int, int, int> color)
{
	Color fg( std::get<0>(color), std::get<1>(color), std::get<2>(color)); 

	rgb_matrix::DrawLine(canvas, x0, y0, x1, y1, fg);
}

void LedMatrix :: DrawLine   (const Nan::FunctionCallbackInfo<Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder()); 
	int x0 = 0; 
	int y0 = 0; 
	int x1 = 0; 
	int y1 = 0; 
	int r = 0; 
	int g = 0; 
	int b = 0; 

	if (args.Length() > 0 && args[0]->IsNumber())
	{
		x0 = args[0]->ToInteger()->Value();
	}

	if (args.Length() > 1 && args[1]->IsNumber())
	{
		y0 = args[1]->ToInteger()->Value();
	}

	if (args.Length() > 2 && args[2]->IsNumber())
	{
		x1 = args[2]->ToInteger()->Value();
	}

	if (args.Length() > 3 && args[3]->IsNumber())
	{
		y1 = args[3]->ToInteger()->Value();
	}

	if (args.Length() > 4 && args[4]->IsNumber())
	{
		r = args[4]->ToInteger()->Value();
	}

	if (args.Length() > 5 && args[5]->IsNumber())
	{
		g = args[5]->ToInteger()->Value();
	}


	if (args.Length() > 6 && args[6]->IsNumber())
	{
		b = args[6]->ToInteger()->Value();
	}

	return matrix->DrawLine( x0, y0, x1, y1, std::make_tuple(r, g, b));
}
void LedMatrix :: Brightness (int b)
{

	matrix->SetBrightness(b);
}

void LedMatrix :: Brightness (const Nan::FunctionCallbackInfo<Value>& args)
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder()); 
	int b = 100; 

	b = args[0]->ToInteger()->Value();

	return matrix->Brightness(b);
}


	


void LedMatrix::New(const Nan::FunctionCallbackInfo<Value>& args) 
{
	// throw an error if it's not a constructor 
	if (!args.IsConstructCall()) 
	{
		Nan::ThrowError("LedMatrix::must be called as a constructor with 'new' keyword");
	}

	// grab parameters
	int rows = 32;
	int cols = 32;
	int chained = 1;
	int parallel = 1;
	int brightness = 100;
	std::string mapping = "regular";
	std::string rgbSeq = "RGB";



	if(args.Length() > 0 && args[0]->IsNumber()) {
		rows = args[0]->ToInteger()->Value();
	}
	if(args.Length() > 1 && args[1]->IsNumber()) {
		cols = args[1]->ToInteger()->Value();
	}

	if(args.Length() > 2 && args[2]->IsNumber()) {
		chained = args[2]->ToInteger()->Value();
	}
	if(args.Length() > 3 && args[3]->IsNumber()) {
		parallel = args[3]->ToInteger()->Value();
	}

	if(args.Length() > 4 && args[4]->IsNumber())  {
		brightness = args[4]->ToInteger()->Value();
	}


	if(args.Length() > 5 && args[5]->IsString()) {

		v8::String::Utf8Value str(args[5]->ToString());
		mapping = std::string(*str);
	}

	if(args.Length() > 6 && args[6]->IsString()) {

		v8::String::Utf8Value str(args[6]->ToString());
		rgbSeq = std::string(*str);
	}

	std::vector<std::string> strings; 

	Handle<Value> val;
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handle_scope(isolate);

	if(args.Length() > 7 && args[7]->IsArray()) 
	{
		Handle<Array> jsArray = Handle<Array>::Cast(args[7]);
		for(unsigned int i = 0; i < jsArray->Length(); i++)
		{
			val = jsArray->Get(v8::Integer::New(isolate, i));
			strings.push_back(std::string( *String::Utf8Value(val)));		
		}
	}

	// make the matrix
	LedMatrix* matrix = new LedMatrix(rows, cols, chained, parallel, brightness,  mapping.c_str(), rgbSeq.c_str(), strings);
	matrix->Wrap(args.This());

	// return this object
	args.GetReturnValue().Set(args.This());
}

void LedMatrix::GetWidth (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());
	args.GetReturnValue().Set(Nan::New<v8::Number>(matrix->GetWidth()));
}

void LedMatrix::GetHeight (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());
	args.GetReturnValue().Set(Nan::New<v8::Number>(matrix->GetHeight()));
}

void LedMatrix::SetPixel (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());

	if(args.Length() != 5 || !args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()
	|| !args[3]->IsNumber() || !args[4]->IsNumber()) {
		Nan::ThrowTypeError("Wrong parameters! Expects 5 numbers");
  	}

  	int x = args[0]->ToInteger()->Value();
  	int y = args[1]->ToInteger()->Value();
  	int r = args[2]->ToInteger()->Value();
  	int g = args[3]->ToInteger()->Value();
  	int b = args[4]->ToInteger()->Value();

  	matrix->SetPixel(x, y, r, g, b);
}

void LedMatrix::Clear (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());

	if(args.Length() == 4 && args[0]->IsNumber() && args[1]->IsNumber() && args[2]->IsNumber() && args[3]->IsNumber()) {
		int x = args[0]->ToInteger()->Value();
	  	int y = args[1]->ToInteger()->Value();
	  	int w = args[2]->ToInteger()->Value();
	  	int h = args[3]->ToInteger()->Value();
	  	matrix->Clear(x, y, w, h);
	} else {
		matrix->Clear();
	}
}

void LedMatrix::Fill (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());

	if(args.Length() != 3 || !args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
		Nan::ThrowTypeError("Wrong parameters! Expects 3 numbers");
	}

	int r = args[0]->ToInteger()->Value();
  	int g = args[1]->ToInteger()->Value();
  	int b = args[2]->ToInteger()->Value();
  	matrix->Fill(r, g, b);
}

void LedMatrix::SetImageBuffer (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	if(args.Length() < 3 || !args[1]->IsNumber() || !args[2]->IsNumber()) {
		return Nan::ThrowTypeError("Parameters error: SetImageBuffer(buffer, width, height)");
	}

	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());

	v8::Local<v8::Context> context = Nan::GetCurrentContext();
	char* data = Buffer::Data(args[0]->ToObject(context).ToLocalChecked());
	size_t length = Buffer::Length(args[0]->ToObject(context).ToLocalChecked());
	int width = Nan::To<int>(args[1]).FromJust();
	int height = Nan::To<int>(args[2]).FromJust();
	int amount = width * height;
	assert(length == (size_t) amount * 3 || length == (size_t) amount * 4);
	int step = length / amount;

	Image img;
	Pixel* pixels = new Pixel[amount];
	int j = 0;
	for(int i = 0; i < amount; i++) {
		int r = data[j + 0];
		int g = data[j + 1];
		int b = data[j + 2];
		if (step == 4) {
			// Handle Alpha value.
			int a = data[j + 3];
			r = r * a / 255;
			g = g * a / 255;
			b = b * a / 255;
		}
		pixels[i] = Pixel(r, g, b);
		j += step;
	}

	img.SetPixels(pixels);
	img.SetWidth(width);
	img.SetHeight(height);

	matrix->SetImage(&img);
}

void LedMatrix::Draw (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.This());

	int startx = 0;
	int starty = 0;
	if(args.Length() > 0 && args[0]->IsNumber()) startx = args[0]->ToInteger()->Value();
	if(args.Length() > 1 && args[1]->IsNumber()) starty = args[1]->ToInteger()->Value();

	int width = matrix->GetWidth() - startx;
	int height = matrix->GetHeight() - starty;
	int imgx = 0;
	int imgy = 0;
	bool looph = false;
	bool loopv = false;

	if(args.Length() > 2 && args[2]->IsNumber()) width = args[2]->ToInteger()->Value();
	if(args.Length() > 3 && args[3]->IsNumber()) height = args[3]->ToInteger()->Value();
	if(args.Length() > 4 && args[4]->IsNumber()) imgx = args[4]->ToInteger()->Value();
	if(args.Length() > 5 && args[5]->IsNumber()) imgy = args[5]->ToInteger()->Value();
	if(args.Length() > 6 && args[6]->IsBoolean()) looph = args[6]->ToBoolean()->Value();
	if(args.Length() > 7 && args[7]->IsBoolean()) loopv = args[7]->ToBoolean()->Value();


    matrix->Draw(startx, starty, width, height, imgx, imgy, looph, loopv);
}

void LedMatrix::Scroll (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.This());

	if(args.Length() == 0 || !args[0]->IsFunction()) {
    	return Nan::ThrowTypeError("Callback is required and must be a Function");
  	}

	int startx = 0;
	int starty = 0;
	if(args.Length() > 1 && args[1]->IsNumber()) startx = args[1]->ToInteger()->Value();
	if(args.Length() > 2 && args[2]->IsNumber()) starty = args[2]->ToInteger()->Value();

	int width = matrix->GetWidth() - startx;
	int height = matrix->GetHeight() - starty;
	int scroll = SCROLL_TO_LEFT;
	int speed = 1;
	int loop = 0;

	if(args.Length() > 3 && args[3]->IsNumber()) width = 	args[3]->ToInteger()->Value();
	if(args.Length() > 4 && args[4]->IsNumber()) height = 	args[4]->ToInteger()->Value();
	if(args.Length() > 5 && args[5]->IsNumber()) scroll = 	args[5]->ToInteger()->Value();
	if(args.Length() > 6 && args[6]->IsNumber()) speed = 	args[6]->ToInteger()->Value();
	if(args.Length() > 7 && args[7]->IsNumber()) loop = 	args[7]->ToInteger()->Value();

	// convert to ms
	speed = speed * 1000;

	uvscroll* uv = new uvscroll();
	uv->matrix = matrix;	uv->callback = new Nan::Callback(Local<Function>::Cast(args[0]));
	uv->startx = startx;	uv->starty = starty;
	uv->width = width;		uv->height = height;
	uv->scroll = scroll;	uv->loop = loop;
	uv->speed = speed;

	matrix->Ref();

	uv_work_t* w = new uv_work_t;
	w->data = uv;
	int r = uv_queue_work(uv_default_loop(), w, UV_Scroll, UV_AfterScroll);
	if(r != 0) {
		delete uv;
		delete w;

		matrix->Unref();

		return Nan::ThrowTypeError("Internal error: Failed to queue scroll work");
	}
}

void LedMatrix::Update (const Nan::FunctionCallbackInfo<v8::Value>& args) 
{
	LedMatrix* matrix = ObjectWrap::Unwrap<LedMatrix>(args.Holder());

	matrix->Update();

}

void LedMatrix::UV_Scroll (uv_work_t* work) 
{
	uvscroll* uv = static_cast<uvscroll*>(work->data);

	assert(uv->matrix != NULL);

	if(uv->scroll == SCROLL_TO_LEFT) {
		if(uv->loop > 0) {
			for(int l = 0; l < uv->loop; l++) {
				for(int i = 0; i < uv->matrix->image->GetWidth(); i++) {
					uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
					uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, i, 0, true, false);
					usleep(uv->speed);
				}
				uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
				uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, 0, 0, true, false);
			}
		} else {
			for(int i = 0; i < uv->matrix->image->GetWidth(); i++) {
				uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
				uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, i, 0, false, false);
				usleep(uv->speed);
			}
		}
	} else if(uv->scroll == SCROLL_TO_RIGHT) {
		if(uv->loop < 1) {
			for(int i = uv->startx; i < uv->width; i++) {
				uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
				uv->matrix->Draw(i, uv->starty, uv->width-i, uv->height, 0, 0, false, false);
				usleep(uv->speed);
			}
		} else {
			for(int l = 0; l < uv->loop; l++) {
				for(int i = uv->matrix->image->GetWidth()-1; i >= 0 ; i--) {
					uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
					uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, i, 0, true, false);
					usleep(uv->speed);
				}
			}
		}
	} else if(uv->scroll == SCROLL_TO_TOP) {
		if(uv->loop > 0) {
			for(int l = 0; l < uv->loop; l++) {
				for(int i = 0; i < uv->matrix->image->GetHeight(); i++) {
					uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
					uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, 0, i, false, true);
					usleep(uv->speed);
				}
				uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
				uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, 0, 0, false, true);	
			}
		} else {
			for(int i = 0; i < uv->matrix->image->GetHeight(); i++) {
				uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, 0, i, false, false);
				usleep(uv->speed);
			}
		}
	} else if(uv->scroll == SCROLL_TO_BOTTOM) {
		if(uv->loop < 1) {
			for(int i = uv->starty; i < uv->height; i++) {
				uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
				uv->matrix->Draw(uv->startx, i, uv->width, uv->height-1, 0, 0, false, false);
				usleep(uv->speed);
			}
		} else {
			for(int l = 0; l < uv->loop; l++) {
				for(int i = uv->matrix->image->GetHeight()-1; i >= 0 ; i--) {
					uv->matrix->Clear(uv->startx, uv->starty, uv->width, uv->height);
					uv->matrix->Draw(uv->startx, uv->starty, uv->width, uv->height, 0, i, false, true);
					usleep(uv->speed);
				}
			}
		}
	}
}

void LedMatrix::UV_AfterScroll (uv_work_t* work, int status) 
{
	uvscroll* uv = static_cast<uvscroll*>(work->data);
	uv->matrix->Unref();
	uv->callback->Call(0, 0);
	delete uv;
	delete work;
}
