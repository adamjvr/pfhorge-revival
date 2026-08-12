//
//  OpenGLVisualModeController.m
//  Pfhorge
//
//  Created by Joshua D. Orr on Sat Feb 23 2002.
//  Copyright (c) 2001 Joshua D. Orr. All rights reserved.
//  
//  E-Mail:   dragons@xmission.com
//  
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.

//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  or you can read it by running the program and selecting Phorge->About Phorge


#import "OpenGLVisualModeController.h"
#import "MyOpenGLView2.h"
#include "LEExtras.h"
#include "../Preview/UI/PfhorgeForgeVisualModeWorkspace.inc"

@implementation OpenGLVisualModeController

// *********************** Class Methods ***********************
/*+ (id)sharedOpenGLVisualModeController {
    static OpenGLVisualModeController *sharedOpenGLVisualModeController = nil;

    if (!sharedOpenGLVisualModeController) {
        sharedOpenGLVisualModeController = [[OpenGLVisualModeController alloc] init];
    }

    return sharedOpenGLVisualModeController;
}*/

// *********************** Overridden/Regular Methods ***********************
#pragma mark - Overridden Methods

- (id)initWithLevelData:(LELevelData *)theLevel
{
    self = [super initWithWindowNibName:@"OpenGLVisualMode"];
    
    if (self == nil)
        return nil;
    
    levelData = theLevel;
    
    return self;
}

/*- (void)awakeFromNib
{
    ///[super awakeFromNib];
    //[self showWindow:nil];
}*/

- (void)dealloc
{
    ///[[NSNotificationCenter defaultCenter] removeObserver:self];
    
    levelData = nil;
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    NSDictionary<NSString *, NSString *> *environment =
        NSProcessInfo.processInfo.environment;

    BOOL useMetalPreview =
        [environment[@"PFHORGE_METAL_PREVIEW"] boolValue] ||
        [[NSUserDefaults standardUserDefaults]
            boolForKey:@"PfhorgeUseMetalPreview"];

    if (useMetalPreview) {
        // VM-UI-1A: one native macOS window now hosts both the Metal viewport
        // and a Forge-inspired AppKit texture palette. The palette is
        // selection-only in this phase; renderer/map semantics are untouched.
        PfhorgeConfigureMetalVisualModeWindow(self.window);

        NSView *contentView = self.window.contentView;
        NSRect metalInitialFrame = contentView.bounds;
        metalInitialFrame.size.height =
            MAX(
                180.0,
                metalInitialFrame.size.height -
                PfhorgeVMForgePaletteHeight -
                PfhorgeVMForgeSeparatorHeight);

        PfhorgeMetalPreviewView *metalView =
            [[PfhorgeMetalPreviewView alloc]
                initWithFrame:metalInitialFrame
                    levelData:levelData];

        if (metalView != nil) {
            PfhorgeForgeVisualModeWorkspaceView *workspace =
                [[PfhorgeForgeVisualModeWorkspaceView alloc]
                    initWithFrame:contentView.bounds
                        metalView:metalView
                        levelData:levelData];

            self.window.contentView = workspace;
            self.window.title =
                [self.window.title
                    stringByAppendingString:
                    @" — Metal Portal Preview VM-3"];

            [self.window makeFirstResponder:metalView];
            return;
        }

        NSLog(
            @"Metal Visual Mode could not initialize; "
             "falling back to legacy OpenGL.");
    }

    [OpenGLViewOGLV
        doMapRenderingLoopWithMapData:levelData
                       shapesLocation:
        [preferences URLForKey:VMShapesPath].path];
}


@end
