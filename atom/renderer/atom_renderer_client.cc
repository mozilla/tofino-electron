// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/renderer/atom_renderer_client.h"

#include <string>
#include <vector>

#include "atom/common/api/api_messages.h"
#include "atom/common/api/atom_bindings.h"
#include "atom/common/api/event_emitter_caller.h"
#include "atom/common/color_util.h"
#include "atom/common/native_mate_converters/value_converter.h"
#include "atom/common/node_bindings.h"
#include "atom/common/node_includes.h"
#include "atom/common/options_switches.h"
#include "atom/renderer/atom_render_view_observer.h"
#include "atom/renderer/guest_view_container.h"
#include "atom/renderer/node_array_buffer_bridge.h"
#include "atom/renderer/preferences_manager.h"
#include "base/command_line.h"
#include "chrome/renderer/media/chrome_key_systems.h"
#include "chrome/renderer/pepper/pepper_helper.h"
#include "chrome/renderer/printing/print_web_view_helper.h"
#include "chrome/renderer/tts_dispatcher.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message_macros.h"
#include "native_mate/dictionary.h"
#include "third_party/WebKit/public/web/WebCustomElement.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#endif

#if defined(OS_WIN)
#include <shlobj.h>
#endif

namespace atom {

namespace {

// Helper class to forward the messages to the client.
class AtomRenderFrameObserver : public content::RenderFrameObserver {
 public:
  AtomRenderFrameObserver(content::RenderFrame* frame,
                          AtomRendererClient* renderer_client)
      : content::RenderFrameObserver(frame),
        render_frame_(frame),
        world_id_(-1),
        renderer_client_(renderer_client) {}

  // content::RenderFrameObserver:
  void DidClearWindowObject() override {
    renderer_client_->DidClearWindowObject(render_frame_);
  }

  void DidCreateScriptContext(v8::Handle<v8::Context> context,
                              int extension_group,
                              int world_id) override {
    if (world_id_ != -1 && world_id_ != world_id)
      return;
    world_id_ = world_id;
    renderer_client_->DidCreateScriptContext(context, render_frame_);
  }
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override {
    if (world_id_ != world_id)
      return;
    renderer_client_->WillReleaseScriptContext(context, render_frame_);
  }

 private:
  content::RenderFrame* render_frame_;
  int world_id_;
  AtomRendererClient* renderer_client_;

  DISALLOW_COPY_AND_ASSIGN(AtomRenderFrameObserver);
};

v8::Local<v8::Value> GetRenderProcessPreferences(
    const PreferencesManager* preferences_manager, v8::Isolate* isolate) {
  if (preferences_manager->preferences())
    return mate::ConvertToV8(isolate, *preferences_manager->preferences());
  else
    return v8::Null(isolate);
}

void AddRenderBindings(v8::Isolate* isolate,
                       v8::Local<v8::Object> process,
                       const PreferencesManager* preferences_manager) {
  mate::Dictionary dict(isolate, process);
  dict.SetMethod(
      "getRenderProcessPreferences",
      base::Bind(GetRenderProcessPreferences, preferences_manager));
}

bool IsDevToolsExtension(content::RenderFrame* render_frame) {
  return static_cast<GURL>(render_frame->GetWebFrame()->document().url())
      .SchemeIs("chrome-extension");
}

}  // namespace

AtomRendererClient::AtomRendererClient()
    : node_bindings_(NodeBindings::Create(false)),
      atom_bindings_(new AtomBindings) {
  // Parse --standard-schemes=scheme1,scheme2
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string custom_schemes = command_line->GetSwitchValueASCII(
      switches::kStandardSchemes);
  if (!custom_schemes.empty()) {
    std::vector<std::string> schemes_list = base::SplitString(
        custom_schemes, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const std::string& scheme : schemes_list)
      url::AddStandardScheme(scheme.c_str(), url::SCHEME_WITHOUT_PORT);
  }
}

AtomRendererClient::~AtomRendererClient() {
}

void AtomRendererClient::RenderThreadStarted() {
  blink::WebCustomElement::addEmbedderCustomElementName("webview");
  blink::WebCustomElement::addEmbedderCustomElementName("browserplugin");

  OverrideNodeArrayBuffer();

  preferences_manager_.reset(new PreferencesManager);

#if defined(OS_WIN)
  // Set ApplicationUserModelID in renderer process.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::string16 app_id =
      command_line->GetSwitchValueNative(switches::kAppUserModelId);
  if (!app_id.empty()) {
    SetCurrentProcessExplicitAppUserModelID(app_id.c_str());
  }
#endif

#if defined(OS_MACOSX)
  // Disable rubber banding by default.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kScrollBounce)) {
    base::ScopedCFTypeRef<CFStringRef> key(
        base::SysUTF8ToCFStringRef("NSScrollViewRubberbanding"));
    base::ScopedCFTypeRef<CFStringRef> value(
        base::SysUTF8ToCFStringRef("false"));
    CFPreferencesSetAppValue(key, value, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
  }
#endif
}

void AtomRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new PepperHelper(render_frame);
  new AtomRenderFrameObserver(render_frame, this);

  // Allow file scheme to handle service worker by default.
  // FIXME(zcbenz): Can this be moved elsewhere?
  blink::WebSecurityPolicy::registerURLSchemeAsAllowingServiceWorkers("file");
}

void AtomRendererClient::RenderViewCreated(content::RenderView* render_view) {
  new printing::PrintWebViewHelper(render_view);
  new AtomRenderViewObserver(render_view, this);

  blink::WebFrameWidget* web_frame_widget = render_view->GetWebFrameWidget();
  if (!web_frame_widget)
    return;

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kGuestInstanceID)) {  // webview.
    web_frame_widget->setBaseBackgroundColor(SK_ColorTRANSPARENT);
  } else {  // normal window.
    // If backgroundColor is specified then use it.
    std::string name = cmd->GetSwitchValueASCII(switches::kBackgroundColor);
    // Otherwise use white background.
    SkColor color = name.empty() ? SK_ColorWHITE : ParseHexColor(name);
    web_frame_widget->setBaseBackgroundColor(color);
  }
}

void AtomRendererClient::DidClearWindowObject(
    content::RenderFrame* render_frame) {
  // Make sure every page will get a script context created.
  render_frame->GetWebFrame()->executeScript(blink::WebScriptSource("void 0"));
}

void AtomRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  // Inform the document start pharse.
  node::Environment* env = node_bindings_->uv_env();
  if (env) {
    v8::HandleScope handle_scope(env->isolate());
    mate::EmitEvent(env->isolate(), env->process_object(), "document-start");
  }
}

void AtomRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  // Inform the document end pharse.
  node::Environment* env = node_bindings_->uv_env();
  if (env) {
    v8::HandleScope handle_scope(env->isolate());
    mate::EmitEvent(env->isolate(), env->process_object(), "document-end");
  }
}

blink::WebSpeechSynthesizer* AtomRendererClient::OverrideSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return new TtsDispatcher(client);
}

bool AtomRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (params.mimeType.utf8() == content::kBrowserPluginMimeType ||
      command_line->HasSwitch(switches::kEnablePlugins))
    return false;

  *plugin = nullptr;
  return true;
}

void AtomRendererClient::DidCreateScriptContext(
    v8::Handle<v8::Context> context, content::RenderFrame* render_frame) {
  // Only allow node integration for the main frame, unless it is a devtools
  // extension page.
  if (!render_frame->IsMainFrame() && !IsDevToolsExtension(render_frame))
    return;

  // Whether the node binding has been initialized.
  bool first_time = node_bindings_->uv_env() == nullptr;

  // Prepare the node bindings.
  if (first_time) {
    node_bindings_->Initialize();
    node_bindings_->PrepareMessageLoop();
  }

  // Setup node environment for each window.
  node::Environment* env = node_bindings_->CreateEnvironment(context);

  // Add atom-shell extended APIs.
  atom_bindings_->BindTo(env->isolate(), env->process_object());
  AddRenderBindings(env->isolate(), env->process_object(),
                    preferences_manager_.get());

  // Load everything.
  node_bindings_->LoadEnvironment(env);

  if (first_time) {
    // Make uv loop being wrapped by window context.
    node_bindings_->set_uv_env(env);

    // Give the node loop a run to make sure everything is ready.
    node_bindings_->RunMessageLoop();
  }
}

void AtomRendererClient::WillReleaseScriptContext(
    v8::Handle<v8::Context> context, content::RenderFrame* render_frame) {
  // Only allow node integration for the main frame, unless it is a devtools
  // extension page.
  if (!render_frame->IsMainFrame() && !IsDevToolsExtension(render_frame))
    return;

  node::Environment* env = node::Environment::GetCurrent(context);
  if (env)
    mate::EmitEvent(env->isolate(), env->process_object(), "exit");
}

bool AtomRendererClient::ShouldFork(blink::WebLocalFrame* frame,
                                    const GURL& url,
                                    const std::string& http_method,
                                    bool is_initial_navigation,
                                    bool is_server_redirect,
                                    bool* send_referrer) {
  // Handle all the navigations and reloads in browser.
  // FIXME We only support GET here because http method will be ignored when
  // the OpenURLFromTab is triggered, which means form posting would not work,
  // we should solve this by patching Chromium in future.
  *send_referrer = true;
  return http_method == "GET" && !is_server_redirect;
}

content::BrowserPluginDelegate* AtomRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
  if (mime_type == content::kBrowserPluginMimeType) {
    return new GuestViewContainer(render_frame);
  } else {
    return nullptr;
  }
}

void AtomRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
  AddChromeKeySystems(key_systems);
}

}  // namespace atom
