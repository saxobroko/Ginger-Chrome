// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/tab_switching.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/main/connection_information.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

// The controller object for a scene. Reacts to scene state changes.
@interface SceneController : NSObject <SceneStateObserver,
                                       ApplicationCommands,
                                       TabSwitching,
                                       ConnectionInformation,
                                       TabOpening,
                                       WebStateListObserving>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithSceneState:(SceneState*)sceneState
    NS_DESIGNATED_INITIALIZER;

// The state of the scene controlled by this object.
@property(nonatomic, weak, readonly) SceneState* sceneState;

// The interface provider for this scene.
@property(nonatomic, strong, readonly) id<BrowserInterfaceProvider>
    interfaceProvider;

// Handler for the UIWindowSceneDelegate callback with the same selector.
- (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
    API_AVAILABLE(ios(13));

// Return YES if incognito mode is forced by enterprise policy.
- (BOOL)isIncognitoForced;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_H_
