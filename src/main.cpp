#include <Shlwapi.h>
#include <wrl.h>
#include <wil/com.h>
#include "WebView2.h"

#include <napi.h>

#include <iostream>
#include <vector>
#include <map>

using namespace Napi;
using namespace std;
using namespace Microsoft::WRL;

const string WINDOW_CLASS = "WindowC++";
wstring BROWSER_DIR = L"";// L"C:\\Users\\yt040\\Downloads\\Microsoft.WebView2.FixedVersionRuntime.122.0.2365.93.x86";

LPWSTR StringToLPWSTR(const std::string& str) {
	// Get the required buffer size for wide character string
	int bufferSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	if (bufferSize == 0) {
		// Handle error
		std::cerr << "Error in MultiByteToWideChar: " << GetLastError() << std::endl;
		return nullptr;
	}

	// Allocate memory for the wide-character string
	LPWSTR wideStr = new WCHAR[bufferSize];
	if (!wideStr) {
		// Handle memory allocation failure
		std::cerr << "Failed to allocate memory for wide string" << std::endl;
		return nullptr;
	}

	// Convert the string to wide-character string
	if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr, bufferSize) == 0) {
		// Handle error
		std::cerr << "Error in MultiByteToWideChar: " << GetLastError() << std::endl;
		delete[] wideStr; // Free memory in case of failure
		return nullptr;
	}

	return wideStr;
}

struct Response {
	IStream* stream;
	LPWSTR contentType;
	int status = 200;
	LPWSTR statusText;

	~Response() {
		if (stream) {
			stream->Release();
			stream = nullptr;
		}

		delete[] contentType;
		if (statusText) {
			delete[] statusText;
		}
	}
};

class BrowserWindow : public Napi::ObjectWrap<BrowserWindow> {
public:
	static Napi::Function Init(Napi::Env env);

	BrowserWindow(const Napi::CallbackInfo& info);

	// Pointer to WebViewController
	wil::com_ptr <ICoreWebView2Controller> webviewController;

	// Pointer to WebView window
	wil::com_ptr <ICoreWebView2> webview;

	Napi::Reference<Napi::Function> reqHandler;

	Napi::Value send(string eventName, vector<napi_value>& args = vector<napi_value>{}) {
		auto emitter = this->Value();
		auto emitFunc = emitter.Get("emit").As<Napi::Function>();

		args.insert(args.begin(), Napi::String::New(emitFunc.Env(), eventName));

		return emitFunc.Call(emitter, args);
	}

	~BrowserWindow() {
		reqHandler.Reset();
	}

private:
	Napi::Reference<Napi::Value> thisObj;
	HWND handle;

	Napi::Value openDevTools(const Napi::CallbackInfo& info) {
		return Boolean::New(info.Env(), SUCCEEDED(webview->OpenDevToolsWindow()));
	}

	Napi::Value setRequestHandler(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();

		if (info.Length() < 1)
		{
			throw TypeError::New(env, "Expected one arg");
		}
		else if (!info[0].IsFunction())
		{
			throw TypeError::New(env, "Expected 1st arg to be function");
		}

		reqHandler.Reset();
		reqHandler = Napi::Persistent(info[0].As<Function>());
	}

	Napi::Value addRequestFilter(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();

		if (reqHandler.IsEmpty()) {
			throw TypeError::New(env, "Add request handler function first");
		}
		else if (info.Length() < 2)
		{
			throw TypeError::New(env, "Expected two args");
		}
		else if (!info[0].IsString())
		{
			throw TypeError::New(env, "Expected 1st arg to be string");
		}
		else if (!info[1].IsNumber())
		{
			throw TypeError::New(env, "Expected 2nd arg to be number");
		}

		string filter = info[0].As<String>().Utf8Value();
		int type = info[1].As<Number>().Int32Value();

		if (type < 0 || type > 16)
			throw TypeError::New(env, "Expected 2nd arg to be valid resource context (0-16)");

		wstring sfilter = wstring(filter.begin(), filter.end()).c_str();

		HRESULT hr = webview->AddWebResourceRequestedFilter(sfilter.c_str(), (COREWEBVIEW2_WEB_RESOURCE_CONTEXT)type);

		return Boolean::New(info.Env(), SUCCEEDED(hr));
	}
};

HINSTANCE hInstance = GetModuleHandle(nullptr);
map<HWND, BrowserWindow*> windows{};
int windowsCount = 0;

string GetLastErrorAsString() {
	DWORD errorMessageID = GetLastError();
	if (errorMessageID == 0)
		return std::string(); // No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
	std::string message(messageBuffer, size);

	// Free the buffer allocated by FormatMessage
	LocalFree(messageBuffer);

	return message;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	map<HWND, BrowserWindow*>::iterator it = windows.find(hWnd);

	if (it != windows.end())
	{
		it->second->send(to_string(message));
	}

	switch (message)
	{
	case WM_SIZE:
		break;
	case WM_DESTROY:
	{
		PostQuitMessage(0);

		if (it != windows.end())
		{
			delete it->second;

			windows.erase(it);
			windowsCount--;
		}
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

bool CreateClassWindow() {
	WNDCLASSEX wcex;
	if (GetClassInfoEx(hInstance, WINDOW_CLASS.c_str(), &wcex) == 0) {
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = WINDOW_CLASS.c_str();
		wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

		if (!RegisterClassEx(&wcex)) {
			return false;
		}
	}

	return true;
}

HWND CreateNativeWindow(const string& title) {
	HWND hWnd = CreateWindow(
		WINDOW_CLASS.c_str(),
		title.c_str(),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT,
		500, 600,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hWnd) {
		return nullptr;
	}

	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	return hWnd;
}

void InitializeWebView(HWND hWnd, BrowserWindow* bw, const function<void()> callback) {
	// <-- WebView2 sample code starts here -->
	// Step 3 - Create a single WebView within the parent window
	// Locate the browser and set up the environment for WebView
	// mklink /d C:\Users\yt040\Downloads\Microsoft.WebView2.FixedVersionRuntime.122.0.2365.93.x86 "C:\Program Files (x86)\Microsoft\Edge\Application\122.0.2365.92"

	CreateCoreWebView2EnvironmentWithOptions(BROWSER_DIR.c_str(), nullptr, nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[hWnd, bw, callback](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

				// Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
				env->CreateCoreWebView2Controller(hWnd,
					Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[hWnd, bw, env, callback](HRESULT result,
							ICoreWebView2Controller* controller) -> HRESULT {
								if (controller !=
									nullptr) {
									bw->webviewController = controller;
									bw->webviewController->get_CoreWebView2(
										&(bw->webview));
								}

								// Add a few settings for the webview
								// The demo step is redundant since the values are the default settings
								wil::com_ptr <ICoreWebView2Settings> settings;
								bw->webview->get_Settings(&settings);
								settings->put_IsScriptEnabled(TRUE);
								settings->put_AreDefaultScriptDialogsEnabled(TRUE);
								settings->put_IsWebMessageEnabled(TRUE);
								settings->put_AreDefaultContextMenusEnabled(FALSE);

								// Resize WebView to fit the bounds of the parent window
								RECT bounds;
								GetClientRect(hWnd, &bounds);
								bw->webviewController->put_Bounds(bounds);

								// Schedule an async task to navigate to Google
								bw->webview->Navigate(L"https://google.com/");

								// <NavigationEvents>
								// Step 4 - Navigation events
								// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
								EventRegistrationToken token;
								//bw->webview->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE);
								bw->webview->add_WebResourceRequested(
									Callback<ICoreWebView2WebResourceRequestedEventHandler>(
										[bw](
											ICoreWebView2* sender,
											ICoreWebView2WebResourceRequestedEventArgs* args) {
												COREWEBVIEW2_WEB_RESOURCE_CONTEXT resourceContext;
												args->get_ResourceContext(&resourceContext);

												wil::com_ptr<ICoreWebView2WebResourceRequest> webReq;
												args->get_Request(&webReq);

												Env env = bw->Env();

												napi_value req = Object::New(env);
												auto res = Object::New(env);
												std::unique_ptr<Response> resObj = nullptr;

												auto resFunc = [&resObj](const Napi::CallbackInfo& info) -> Napi::Value
													{
														Napi::Env env = info.Env();
														size_t argc = info.Length();

														if (argc < 2)
														{
															throw TypeError::New(env, "Expected two args");
														}
														else if (!info[0].IsBuffer())
														{
															throw TypeError::New(env, "Expected 1st arg to be buffer");
														}
														else if (!info[1].IsString())
														{
															throw TypeError::New(env, "Expected 2nd arg to be string");
														}

														void* bufferData = nullptr;
														size_t bufferSize = 0;
														napi_get_buffer_info(env, info[0], &bufferData, &bufferSize);

														HGLOBAL hGlobal = GlobalAlloc(GHND, bufferSize);
														if (!hGlobal) {
															// napi_throw_error(env, nullptr, "Failed to allocate memory");
															return Boolean::New(env, false);
														}

														BYTE* pBuffer = static_cast<BYTE*>(GlobalLock(hGlobal));
														if (!pBuffer) {
															GlobalFree(hGlobal);
															// napi_throw_error(env, nullptr, "Failed to lock memory");
															return Boolean::New(env, false);
														}

														memcpy(pBuffer, bufferData, bufferSize);
														GlobalUnlock(hGlobal);

														resObj = std::unique_ptr<Response>(new Response{});

														// Create IStream from the global memory handle
														HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &resObj->stream);
														if (FAILED(hr)) {
															GlobalFree(hGlobal);
															// napi_throw_error(env, nullptr, "Failed to create stream from buffer");
															return Boolean::New(env, false);
														}

														string str = info[1].As<String>().Utf8Value();
														resObj->contentType = StringToLPWSTR(str);

														if (argc >= 3) {
															resObj->status = info[2].As<Number>().Int32Value();
														}

														if (argc >= 4) {
															str = info[3].As<String>().Utf8Value();
														}
														else str = 'OK';

														// resObj->statusText = &(*new wstring{ str.begin(), str.end() })[0];
														resObj->statusText = StringToLPWSTR(str);

														return Boolean::New(env, true);
													};

												res.Set("send", Function::New(env, resFunc));

												// get url
												wil::unique_cotaskmem_string uri;
												webReq->get_Uri(&uri);
												std::wstring source(uri.get());
												string url{ source.begin(), source.end() };

												napi_set_named_property(env, req, "url", String::New(env, url));
												napi_set_named_property(env, req, "type", Number::New(env, resourceContext));

												bw->reqHandler.Value().As<Function>().Call(vector<napi_value>{req, res});

												wil::com_ptr<IStream> stream = resObj->stream;
												if (!stream) {
													cout << GetLastErrorAsString() << endl;
													return S_OK;
												}

												wil::com_ptr<ICoreWebView2WebResourceResponse> response;
												wil::com_ptr<ICoreWebView2Environment> environment;
												wil::com_ptr<ICoreWebView2_2> webview2;

												HRESULT hr = bw->webview->QueryInterface(IID_PPV_ARGS(&webview2));
												if (FAILED(hr) || !webview2) {
													// Handle error
													cout << GetLastErrorAsString() << endl;
													return S_OK;
												}

												hr = webview2->get_Environment(&environment);
												if (FAILED(hr) || !environment) {
													// Handle error
													cout << GetLastErrorAsString() << endl;
													return S_OK;
												}

												hr = environment->CreateWebResourceResponse(stream.get(), resObj->status, resObj->statusText, resObj->contentType, &response);
												if (FAILED(hr) || !response) {
													// Handle error
													cout << GetLastErrorAsString() << endl;
													return S_OK;
												}

												args->put_Response(response.get());

												stream.reset();
												response.reset();
												environment.reset();
												webview2.reset();

												return S_OK;
										})
									.Get(), &token);

								bw->webview->add_NavigationStarting(
									Callback<ICoreWebView2NavigationStartingEventHandler>(
										[](ICoreWebView2* webview,
											ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
												wil::unique_cotaskmem_string uri;
												args->get_Uri(&uri);
												std::wstring source(uri.get());
												if (source.substr(0, 4) != L"http") {
													args->put_Cancel(true);
												}
												return S_OK;
										}).Get(),
											&token);
								// </NavigationEvents>

								// <Scripting>
								// Step 5 - Scripting
								// Schedule an async task to add initialization script that freezes the Object object
								bw->webview->AddScriptToExecuteOnDocumentCreated(
									L"Object.freeze(Object);",
									nullptr);
								// Schedule an async task to get the document URL
								bw->webview->ExecuteScript(
									L"window.document.URL;",
									Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
										[](HRESULT errorCode,
											LPCWSTR resultObjectAsJson) -> HRESULT {
												LPCWSTR URL = resultObjectAsJson;
												//doSomethingWithURL(URL);
												return S_OK;
										}).Get());
								// </Scripting>

								// <CommunicationHostWeb>
								// Step 6 - Communication between host and web content
								// Set an event handler for the host to return received message back to the web content
								bw->webview->add_WebMessageReceived(
									Callback<ICoreWebView2WebMessageReceivedEventHandler>(
										[](ICoreWebView2* webview,
											ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
												wil::unique_cotaskmem_string message;
												args->TryGetWebMessageAsString(
													&message);
												// processMessage(&message);
												webview->PostWebMessageAsString(
													message.get());
												return S_OK;
										}).Get(),
											&token);

								// Schedule an async task to add initialization script that
								// 1) Add an listener to print message from the host
								// 2) Post document URL to the host
								bw->webview->AddScriptToExecuteOnDocumentCreated(
									L"window.chrome.webview.addEventListener(\'message\', event => alert(event.data));" \
									L"window.chrome.webview.postMessage(window.document.URL);",
									nullptr);
								// </CommunicationHostWeb>

								// cout << "Returned 3\n";
								callback();
								return S_OK;
						}).Get());
				// cout << "Returned 1\n";
				return S_OK;
			}).Get());

	// cout << "Returned 2\n";
	//return 0;
}

Napi::Function BrowserWindow::Init(Napi::Env env) {

	Napi::Function constructor = DefineClass(env, "BrowserWindow",
		{
			InstanceMethod("openDevTools", &BrowserWindow::openDevTools),
			InstanceMethod("setRequestHandler", &BrowserWindow::setRequestHandler),
			InstanceMethod("addRequestFilter", &BrowserWindow::addRequestFilter),
		}
	);

	return constructor;
}

BrowserWindow::BrowserWindow(const Napi::CallbackInfo& info) : Napi::ObjectWrap<BrowserWindow>(info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1)
	{
		throw TypeError::New(env, "Expected one arg");
	}
	else if (!info[0].IsString())
	{
		throw TypeError::New(env, "Expected 1st arg to be string");
	}

	string title = info[0].As<String>().Utf8Value();

	if (CreateClassWindow()) {
		handle = CreateNativeWindow(title);

		if (handle != nullptr) {
			thisObj = Napi::Persistent(info.This());

			windows[handle] = this;
			windowsCount++;

			InitializeWebView(handle, this, [this]()
				{
					this->send("web");
				}
			);

			return;
		}
	}

	throw TypeError::New(env, "Failed to create window");
}

Value SetBrowserLocation(const CallbackInfo& info)
{
	Napi::Env env = info.Env();

	if (info.Length() < 1)
	{
		throw TypeError::New(env, "Expected one arg");
	}
	else if (!info[0].IsString())
	{
		throw TypeError::New(env, "Expected 1st arg to be string");
	}

	string dir = info[0].As<String>().Utf8Value();
	BROWSER_DIR = wstring(dir.begin(), dir.end());
	return env.Undefined();
}

Value MessageLoop(const CallbackInfo& info)
{
	MSG msg;
	while (windowsCount > 0) {
		if (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	UnregisterClass(WINDOW_CLASS.c_str(), hInstance);

	return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set("BrowserWindow", BrowserWindow::Init(env));
	exports.Set("SetBrowserLocation", Function::New(env, SetBrowserLocation));
	exports.Set("MessageLoop", Function::New(env, MessageLoop));

	return exports;
}

NODE_API_MODULE(webview2, Init);