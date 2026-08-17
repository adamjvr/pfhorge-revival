//
//  PhPfhorgeScenarioLevelDoc.m
//  Pfhorge
//
//  Created by Joshua D. Orr on Mon May 27 2002.
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


#import "PhPfhorgeScenarioLevelDoc.h"
#import "PhPfhorgeSingleLevelDoc.h"
#import "LEExtras.h"
#import "LEMapData.h"
#import "ScenarioResources.h"
#import "Resource.h"
#import "Pfhorge-Swift.h"

#include "../Map Intake/Cocoa/PfhorgeMarathonMapIntakeBridge.inc"

// FORMAT-3A: every scenario-created .pfhlev is native.
#include "../Format/Native/PfhorgeNativeDocumentCodec.inc"
#include "../Format/Native/PfhorgeLevelPersistenceBridge.inc"

#import "PhProgress.h"
#include "../Map Intake/Cocoa/PfhorgeMarathonMapImportWorkflow.inc"

#import "LEMapData.h"

#import "PathwaysExchange.h"

@implementation PhPfhorgeScenarioLevelDoc

- (NSImage *)getPICTResourceIndex:(ResID)PICTIndex
{
    /*resource = [resources resourceOfType:@"PICT"
        index:CHAPTER_SCREEN_BASE + [levelPopUp indexOfSelectedItem]];*/
    
    BOOL isDir = NO;
    BOOL exsists = NO;
    NSString *fullImagePath;
    
    NSLog(@"getPICTResourceIndex in the scenerio document called...");
    
    NSMutableArray *tmpImgPaths = [NSMutableArray arrayWithCapacity:5];
    for (NSString *extension in @[@"pict", @"png", @"jpeg", @"jpg", @"bmp"]) {
        fullImagePath  = [[[self fullPathForDirectory]
                                stringByAppendingPathComponent:@"Images/"]
                                stringByAppendingPathComponent:[[@(PICTIndex) stringValue]
                                    stringByAppendingPathExtension:extension]];
        
        [tmpImgPaths addObject:fullImagePath];
    }
    
    for (NSString *path in tmpImgPaths) {
        exsists = [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDir];
        if (exsists) {
            fullImagePath = path;
            break;
        }
    }
    
    if (exsists && !isDir) {
        return [[NSImage alloc] initWithContentsOfFile:fullImagePath];
    } else {
        NSLog(@"image not found at: %@", fullImagePath);
        return nil;//[[[NSImage alloc] initWithData:[resource data]] autorelease];
    }
}

-(id)init
{
    if (self = [super init]) {
        NSString *myFullFilePath = [[self fileURL] URLByDeletingLastPathComponent].path;
        
        if (![myFullFilePath isEqualToString:@"/"]) {
            myFullFilePath  = [myFullFilePath stringByAppendingString:@"/"];
        }
        
        // May want the scenario data boject to get the path dynamicaly from me...
        
        //scenarioData = [[PhScenarioData alloc] initWithProjectDirectory:myFullFilePath];
        scenarioData = nil;
    }
    
    return self;
}

- (void)dealloc
{
    NSLog(@"*** Scenario Dealloc Post Notification ***");
    [[NSNotificationCenter defaultCenter]
     postNotificationName:PhScenarioDeallocatingNotification
     object:self];
}

- (void)openADocumentFile:(NSString *)fullPath
{
    NSLog(@"Attempting To Open: %@", fullPath);
    
	[[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:[NSURL fileURLWithPath:fullPath] display:YES completionHandler:^(NSDocument * _Nullable document, BOOL documentWasAlreadyOpen, NSError * _Nullable error) {
		if (!document) {
			NSLog(@"NIL, error: %@", error);
		}
		[(PhPfhorgeSingleLevelDoc*)document setScenarioDocument:self];
	}];
}

/// + 

- (void)reloadLevelTable:(id)sender
{
    [theScenarioDocumentWindowController reloadLevelTable:sender];
    
    [[NSNotificationCenter defaultCenter]
            postNotificationName:PhScenarioLevelNamesChangedNotification
            object:self];
}

- (IBAction)importMarathonMap:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.allowsMultipleSelection = NO;
    panel.title = NSLocalizedString(@"Import Marathon Map", @"Import Marathon Map");
    panel.prompt = NSLocalizedString(@"Inspect", @"Inspect Marathon map");
    panel.allowedFileTypes = nil;
    panel.allowsOtherFileTypes = YES;

    [panel beginSheetModalForWindow:[self windowForSheet]
                 completionHandler:^(NSModalResponse result) {
        if (result != NSModalResponseOK) {
            return;
        }

        NSURL *fileURL = panel.URL;
        NSData *resolvedMapData = nil;
        NSData *resolvedResourceData = nil;
        PfhMapSourceEnvelopeKind envelopeKind = PfhMapSourceEnvelopeRaw;
        NSString *sourceDescription = nil;
        NSString *finderType = nil;
        NSError *intakeError = nil;

        if (!PfhorgeResolveMarathonSourceURL(
                fileURL,
                &resolvedMapData,
                &resolvedResourceData,
                &envelopeKind,
                &sourceDescription,
                &finderType,
                &intakeError)) {
            [self presentError:intakeError];
            return;
        }

        PfhMapProbeResult mapProbe = {0};
        if (!PfhorgeProbeResolvedMapData(
                resolvedMapData,
                &mapProbe,
                &intakeError)) {
            [self presentError:intakeError];
            return;
        }
        mapProbe.sourceEnvelopeKind = envelopeKind;

        NSIndexSet *selectedEntries = PfhorgeChooseMarathonEntries(
            [self windowForSheet],
            &mapProbe,
            sourceDescription,
            finderType,
            resolvedResourceData.length);
        if (selectedEntries == nil) {
            return;
        }

        NSString *destinationDirectory = [self fullPathForDirectory];
        if (!PfhorgeConfirmSourceDestinationSeparation(
                fileURL,
                destinationDirectory)) {
            return;
        }

        PhProgress *progress = [PhProgress sharedPhProgress];
        [progress setMinProgress:0.0];
        [progress setMaxProgress:100.0];
        [progress setProgressPostion:0.0];
        [progress setStatusText:@"Importing Marathon Data…"];
        [progress setInformationalText:@"Converting selected levels…"];
        [progress showWindow:self];

        NSMutableArray<NSString *> *convertedNames = [NSMutableArray array];
        NSError *conversionError = nil;
        NSArray<LELevelData *> *allConvertedLevels =
            [LEMapData convertMarathonDataToLevels:resolvedMapData
                                        levelNames:convertedNames
                                             error:&conversionError];

        if (allConvertedLevels.count == 0U) {
            [progress orderOutWin:self];
            if (conversionError == nil) {
                conversionError = PfhorgeMapIntakeError(
                    fileURL,
                    @"The Marathon container was structurally valid, but no levels could be converted.");
            }
            [self presentError:conversionError];
            return;
        }

        NSMutableArray<LELevelData *> *selectedLevels = [NSMutableArray array];
        NSMutableArray<NSString *> *originalNames = [NSMutableArray array];
        __block NSError *selectionError = nil;
        [selectedEntries enumerateIndexesUsingBlock:
            ^(NSUInteger ordinal, BOOL *stop) {
                if (ordinal >= allConvertedLevels.count) {
                    selectionError = PfhorgeMapIntakeError(
                        fileURL,
                        [NSString stringWithFormat:
                            @"Selected directory entry %lu was not produced by the semantic converter.",
                            (unsigned long)ordinal]);
                    *stop = YES;
                    return;
                }
                [selectedLevels addObject:allConvertedLevels[ordinal]];
                [originalNames addObject:PfhorgeOriginalLevelName(
                    &mapProbe,
                    ordinal,
                    convertedNames)];
            }];

        if (selectionError != nil) {
            [progress orderOutWin:self];
            [self presentError:selectionError];
            return;
        }

        NSArray<NSString *> *nativeNames = PfhorgeUniqueNativeLevelNames(
            originalNames,
            destinationDirectory);

        [progress setStatusText:@"Writing Selected Levels Atomically…"];
        NSError *writeError = nil;
        if (!PfhorgeStageImportedLevelFiles(
                selectedLevels,
                nativeNames,
                destinationDirectory,
                &writeError)) {
            [progress orderOutWin:self];
            [self presentError:writeError];
            return;
        }

        [progress setStatusText:@"Preserving Source and Provenance…"];
        NSError *snapshotError = nil;
        if (!PfhorgeWriteMarathonImportSnapshot(
                destinationDirectory,
                fileURL,
                resolvedMapData,
                resolvedResourceData,
                &mapProbe,
                envelopeKind,
                selectedEntries,
                originalNames,
                nativeNames,
                &snapshotError)) {
            NSFileManager *manager = NSFileManager.defaultManager;
            for (NSString *nativeName in nativeNames) {
                NSString *path = [[destinationDirectory
                    stringByAppendingPathComponent:nativeName]
                    stringByAppendingPathExtension:@"pfhlev"];
                [manager removeItemAtPath:path error:NULL];
            }
            [progress orderOutWin:self];
            [self presentError:snapshotError];
            return;
        }

        ScenarioResources *marathonResources = nil;
        if (envelopeKind == PfhMapSourceEnvelopeRaw) {
            marathonResources = [[ScenarioResources alloc]
                initWithContentsOfURL:fileURL
                               error:NULL];
        }

        [progress setStatusText:@"Extracting Readable Resources…"];
        NSError *resourceError = nil;
        NSString *imageDirectory =
            [destinationDirectory stringByAppendingPathComponent:@"Images"];
        NSString *soundDirectory =
            [destinationDirectory stringByAppendingPathComponent:@"Sounds"];
        if (!PfhorgeExtractRawScenarioResources(
                marathonResources,
                imageDirectory,
                soundDirectory,
                progress,
                &resourceError)) {
            NSFileManager *manager = NSFileManager.defaultManager;
            for (NSString *nativeName in nativeNames) {
                NSString *path = [[destinationDirectory
                    stringByAppendingPathComponent:nativeName]
                    stringByAppendingPathExtension:@"pfhlev"];
                [manager removeItemAtPath:path error:NULL];
            }
            [progress orderOutWin:self];
            [self presentError:resourceError];
            return;
        }

        [progress setStatusText:@"Adding Levels to Scenario…"];
        [self->scenarioData addLevelNames:nativeNames];
        [progress increaseProgressBy:100.0];
        [progress setStatusText:@"Marathon Import Complete"];
        [progress orderOutWin:self];

        // Save only after a successful, confirmed import. Cancelling the panel or
        // identification dialog never dirties or saves the scenario.
        [self saveDocument:nil];
    }];
}

- (IBAction)importPathwaysMap:(id)sender
{
    NSArray	*fileTypes	= @[NSFileTypeForHFSTypeCode('maps')];
    NSOpenPanel	*op		= [NSOpenPanel openPanel];
    
    [op	setAllowsMultipleSelection:NO];
    [op setTitle:NSLocalizedString(@"Import Pathways Map", @"Import Pathways Map")];
    [op setPrompt:NSLocalizedString(@"Import", @"Import")];
    op.allowedFileTypes = fileTypes;
    
    [op beginSheetModalForWindow:[self windowForSheet] completionHandler:^(NSModalResponse result) {
        NSString        *fileName = nil;
        NSMutableArray  *archivedLevels = [[NSMutableArray alloc] init];
        NSMutableArray  *levelNames = [[NSMutableArray alloc] init];
        NSFileManager   *fileManager = [NSFileManager defaultManager];
        //NSString *imageDir = [[self fullPathForDirectory] stringByAppendingString:@"Images/"];
        
        PhProgress *progress = [PhProgress sharedPhProgress];
        
        if (result == NSModalResponseOK) {
            fileName = op.URL.path;
            
            [progress setMinProgress:0.0];
            [progress setMaxProgress:100.0];
            [progress setProgressPostion:0.0];
            [progress setStatusText:NSLocalizedString(@"Importing Pathways Into Darkness Map…", @"Importing Pathways Into Darkness Map…")];
            [progress setInformationalText:NSLocalizedString(@"Importing Pathways Into Darkness Map…", @"Importing Pathways Into Darkness Map…")];
            [progress showWindow:self];
            
            
            BOOL isDir = NO;
            
            NSString *pathPathwaysApp = [fileName stringByDeletingLastPathComponent];
            NSData *dpin128ResourceData = nil;
            
            pathPathwaysApp = [pathPathwaysApp stringByAppendingPathComponent:@"Pathways Into Darkness"];
            
            BOOL exsists = [fileManager fileExistsAtPath:pathPathwaysApp isDirectory:&isDir];
            
            if ((exsists) && (!isDir)) {
                // Data will be dallocated after fileResources gets released...
                ScenarioResources *fileResources = [[ScenarioResources alloc] initWithContentsOfFile:pathPathwaysApp];
                // Copy data (or we could just retain it, since it's immutable, which is waht the copy method
                //   probabaly does with NSData objects) so we will have it after fileResources gets released...
                dpin128ResourceData = [[[fileResources resourceOfType:@"dpin" index:128] data] copy];
                
                // Should be able to just release it now, but just in case..
                pathPathwaysApp = nil;

                NSLog(@"Was Able To Load 'Pathways Into Darkness' dpin 128 resource...");
            } else {
                pathPathwaysApp = nil;
                dpin128ResourceData = nil;
                
                NSLog(@"Was NOT Able To Load 'Pathways Into Darkness' dpin 128 resource...");
            }
            
            
            [PathwaysExchange convertPIDMapToArchived:[fileManager
                                       contentsAtPath:fileName]
                                               levels:archivedLevels
                                           levelNames:levelNames
                                         resourceData:dpin128ResourceData];
            
            // I don't need this any more, PathwaysExchange should have retained
            // it if it needed it keep it...
            
            [progress setStatusText:@"Saving All The Levels…"];
            
            [self saveArrayOfNSDatas:archivedLevels
                       withFileNames:levelNames
                             baseDir:[self fullPathForDirectory]];
                             
            [progress setStatusText:@"Adding Level Names To Scenario Document…"];
            
            [self->scenarioData addLevelNames:levelNames];
            
            [progress increaseProgressBy:1.0];
            [progress setStatusText:@"Done Converting Level!"];
            [progress orderOutWin:self];
        }
        
        [self saveDocument:nil];
    }];
}

- (IBAction)cut:(id)sender
{

}
- (IBAction)copy:(id)sender
{

}
- (IBAction)paste:(id)sender
{

}


// ****************** Document Overidden Methods ******************
#pragma mark - Document Overidden Methods

- (void)makeWindowControllers
{
    theScenarioDocumentWindowController = [[PhScenarioManagerController alloc] init];
    [self addWindowController:theScenarioDocumentWindowController];
}

- (void)windowControllerDidLoadNib:(NSWindowController *) aController
{
    if (scenarioData == nil)
    {
        scenarioData = [[PhScenarioData alloc] initWithProjectDirectory:@""];
        [self saveDocumentWithDelegate:self didSaveSelector:@selector(document:didSave:contextInfo:) contextInfo:NULL];
        NSLog(@"*** *** ***");
    }
    else
    {
        [scenarioData setProjectDirectory:[self fullPathForDirectory]];
        [scenarioData setTheScenarioDocument:self];
    }
}

- (void)document:(NSDocument *)doc didSave:(BOOL)didSave contextInfo:(void *)contextInfo
{
    if (scenarioData != nil && didSave == NO) // Make Sure There Are No Sheets...
        [self close];
    else if (scenarioData != nil && didSave == YES) // New Doc Did Get Saved...
    {
        [scenarioData setProjectDirectory:[self fullPathForDirectory]];
        [scenarioData setTheScenarioDocument:self];
        
        [theScenarioDocumentWindowController setupDataSourceForLevelTable];
    }
    else {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Generic Error", @"Generic Error");
        alert.informativeText = NSLocalizedString(@"scenarioData == nil, but why am I in the \"document:didSave:contextInfo:\"?", @"scenarioData == nil, but why am I in the \"document:didSave:contextInfo:\"?");
        alert.alertStyle = NSAlertStyleCritical;
        [alert runModal];
    }
}

- (BOOL)prepareSavePanel:(NSSavePanel *)savePanel
{
    /// NSLog(@"SAVING PANEL PERPARATION");
    return YES;
}

- (NSData *)dataOfType:(NSString *)typeName error:(NSError * _Nullable *)outError {
    return [NSKeyedArchiver archivedDataWithRootObject:scenarioData requiringSecureCoding:YES error:outError];
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError * _Nullable *)outError {
    scenarioData = [NSKeyedUnarchiver unarchivedObjectOfClass:[PhScenarioData class] fromData:data error:outError];
	if (!scenarioData) {
		scenarioData = [NSUnarchiver unarchiveObjectWithData:data];
	}
    
    return (scenarioData == nil) ? (NO) : (YES);
}

/*
- (NSDictionary *)fileAttributesToWriteToFile:(NSString *)fullDocumentPath ofType:(NSString *)documentTypeName saveOperation:(NSSaveOperationType)saveOperationType
{
    return [super fileAttributesToWriteToFile:fullDocumentPath ofType:documentTypeName saveOperation:saveOperationType];
}
*/
// ****************** Utilites ******************
#pragma mark - Utilites

- (BOOL)exportLevelToMarathonMap:(NSString *)fullPath error:(NSError **)outError
{
    return [self exportLevelToMarathonMapToURL:[NSURL fileURLWithPath:fullPath] error:outError];
}

- (BOOL)exportLevelToMarathonMapToURL:(NSURL *)fullPath error:(NSError**)outError
{
    NSString *fileName = [scenarioData getLevelPathForSelected];
    NSData *theFileData = [NSData dataWithContentsOfFile:fileName options:0 error:outError];
    if (!theFileData) {
        return NO;
    }
    LELevelData *theLevel =  [NSKeyedUnarchiver unarchivedObjectOfClass:
                              [LELevelData class] fromData:
                              [theFileData subdataWithRange:NSMakeRange(10, ([theFileData length] - 10))] error:outError];
    
    if (theLevel == nil) {
        return NO;
    }
    
    NSData *tempData = [LEMapData convertLevelToDataObject:theLevel error:outError];
    
    if (tempData == nil) {
        return NO;
    }

    BOOL success = [tempData writeToURL:fullPath options:0 error:outError];
    if (!success) {
        return NO;
    }
    
    NSError *tmpErr;
    success = [[NSFileManager defaultManager] setAttributes:
               @{NSFileHFSCreatorCode: @((OSType)0x32362EB0), // '26.∞'
                 NSFileHFSTypeCode: @((OSType)'sce2')
               } ofItemAtPath:fullPath.path error:&tmpErr];
    if (!success) {
        //Just write the error out into the log
        NSLog(@"Unable to set file attributes: %@", tmpErr);
    }
    
    return YES;
}

- (void)rescanProjectDirectoryNow
{
    [scenarioData scanProjectDirectory];
    [self reloadLevelTable:nil];
}

- (NSURL *)fullPathURLForDirectory
{
	return [[self fileURL] URLByDeletingLastPathComponent];
}
- (NSString *)fullPathForDirectory
{
    return self.fullPathURLForDirectory.path;
}

- (void)saveArrayOfNSDatas:(NSArray *)theDataObjs withFileNames:(NSArray *)theFileNames baseDir:(NSString *)basePath
{
    NSEnumerator *dataNumer, *fileNumer;
    NSData *fdata;
    NSString *fname;
    NSString *fpath;
    
    dataNumer = [theDataObjs objectEnumerator];
    fileNumer = [theFileNames objectEnumerator];
    while ((fdata = [dataNumer nextObject]) && 
           (fname = [fileNumer nextObject]))
    {
        fpath = [[basePath stringByAppendingPathComponent:fname] stringByAppendingPathExtension:@"pfhlev"];
        
        NSError *migrationError = nil;
        NSData *nativeData =
            PfhorgeNativePackageFromLegacyPfhlevData(
                fdata,
                &migrationError);

        if (nativeData == nil) {
            NSLog(@"FORMAT-3A Pathways level migration failed for %@: %@",
                  fname,
                  migrationError);
            [self presentError:migrationError];
            return;
        }

        [[NSFileManager defaultManager] createFileAtPath:fpath
                                                contents:nativeData
                                              attributes:@{NSFileHFSTypeCode: @((OSType)'PfhL'),
                                                           NSFileHFSCreatorCode: @((OSType)'PFrg')}];
    }
}

- (BOOL)saveMergedMapToPath:(NSString *)fullPath error:(NSError**)outError
{
    return [self saveMergedMapToURL:[NSURL fileURLWithPath:fullPath] error:outError];
}

- (BOOL)saveMergedMapToURL:(NSURL *)fullPath error:(NSError**)outError
{
    NSData *mergedMap = [LEMapData mergeScenarioToMarathonMapFile:self error:outError];
    if (!mergedMap) {
        return NO;
    }
    NSFileManager *manager = [NSFileManager defaultManager];
    NSArray *subpaths;
    NSEnumerator *numer;
    NSString *fileName;
    BOOL isDir = YES;
    BOOL exsists = NO;
    BOOL success;
    NSString *fullImageDirPath = nil;
    ScenarioResources *maraResources;
    
    if (![mergedMap writeToURL:fullPath options:0 error:outError]) {
        return NO;
    }
    NSError *tmpErr;
    success = [[NSFileManager defaultManager] setAttributes:
     @{NSFileHFSCreatorCode: @((OSType)0x32362EB0), // '26.∞'
       NSFileHFSTypeCode: @((OSType)'sce2')
     } ofItemAtPath:fullPath.path error:&tmpErr];
    if (!success) {
        //Just write the error out into the log
        NSLog(@"Unable to set file attributes: %@", tmpErr);
    }
    
    NSLog(@"Scaning Images folder for resources now...");
    
    fullImageDirPath  = [[self fullPathForDirectory] stringByAppendingPathComponent:@"Images/"];
    NSURL *fullSoundDirPath  = [[self fullPathURLForDirectory] URLByAppendingPathComponent:@"Sound/"];
    
    exsists = [manager fileExistsAtPath:fullImageDirPath isDirectory:&isDir];
    
    if (!exsists || !isDir) {
        if (outError) {
            *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileNoSuchFileError userInfo:@{NSFilePathErrorKey: fullImageDirPath}];
        }
        return NO;
    }
    
    maraResources = [[ScenarioResources alloc] initWithContentsOfURL:fullPath error:outError];
    if (!maraResources) {
        return NO;
    }
    
    subpaths = [manager contentsOfDirectoryAtPath:fullImageDirPath error:NULL];
    numer = [subpaths objectEnumerator];
    while (fileName = [numer nextObject]) {
        NSString *fullResourcePath = [fullImageDirPath stringByAppendingPathComponent:fileName];
        
        if (IsPathDirectory(manager, fullResourcePath)) {
            continue;
        } else if ([[fileName pathExtension] isEqualToString:@"pict"]) {
            ResID thePictResourceNumber = [[fileName stringByDeletingPathExtension] intValue];
            Resource *theResource;
            
            if (thePictResourceNumber < 128) {
                continue;
            }
            
            NSError *err;
            NSData *pictData = [NSData dataWithContentsOfFile:fullResourcePath options:0 error:&err];
            if (!pictData) {
                NSLog(@"PICT read failed: %@", err);
                continue;
            }

            theResource = [[Resource alloc] initWithID:thePictResourceNumber type:@"PICT" name:@""];
            theResource.data = pictData;
            [maraResources addResource:theResource];
        } else if ([[fileName pathExtension] isEqualToString:@"png"]) {
            ResID thePictResourceNumber = [[fileName stringByDeletingPathExtension] intValue];
            Resource *theResource;
            
            if (thePictResourceNumber < 128) {
                continue;
            }
            if ([maraResources resourceOfType:@"PICT" index:thePictResourceNumber load:NO] != nil) {
                continue;
            }
            
            NSError *err;
            NSData *pictData = [PhPictConversion convertFileAtURLToPICT:[NSURL fileURLWithPath:fullResourcePath] error:&err];
            if (!pictData) {
                NSLog(@"PNG to PICT conversion failed: %@", err);
                continue;
            }
            theResource = [[Resource alloc] initWithID:thePictResourceNumber type:@"PICT" name:@""];
            theResource.data = pictData;
            [maraResources addResource:theResource];
        } else if ([[fileName pathExtension] isEqualToString:@"jpg"] || [[fileName pathExtension] isEqualToString:@"jpeg"]) {
            ResID thePictResourceNumber = [[fileName stringByDeletingPathExtension] intValue];
            Resource *theResource;
            
            if (thePictResourceNumber < 128) {
                continue;
            }
            if ([maraResources resourceOfType:@"PICT" index:thePictResourceNumber load:NO] != nil) {
                continue;
            }
            
            NSError *err;
            NSData *pictData = [PhPictConversion convertFileAtURLToPICT:[NSURL fileURLWithPath:fullResourcePath] error:&err];
            if (!pictData) {
                NSLog(@"Jpeg to PICT conversion failed: %@", err);
                continue;
            }
            theResource = [[Resource alloc] initWithID:thePictResourceNumber type:@"PICT" name:@""];
            theResource.data = pictData;
            [maraResources addResource:theResource];
        } else if ([[fileName pathExtension] isEqualToString:@"bmp"]) {
            ResID thePictResourceNumber = [[fileName stringByDeletingPathExtension] intValue];
            Resource *theResource;
            
            if (thePictResourceNumber < 128) {
                continue;
            }
            if ([maraResources resourceOfType:@"PICT" index:thePictResourceNumber load:NO] != nil) {
                continue;
            }
            
            NSError *err;
            NSData *pictData = [PhPictConversion convertFileAtURLToPICT:[NSURL fileURLWithPath:fullResourcePath] error:&err];
            if (!pictData) {
                NSLog(@"Bitmap to PICT conversion failed: %@", err);
                continue;
            }
            theResource = [[Resource alloc] initWithID:thePictResourceNumber type:@"PICT" name:@""];
            theResource.data = pictData;
            [maraResources addResource:theResource];
        }
    }
    
    exsists = [manager fileExistsAtPath:fullSoundDirPath.path isDirectory:&isDir];
    if (exsists && isDir) {
        subpaths = [manager contentsOfDirectoryAtURL:fullSoundDirPath includingPropertiesForKeys:nil options:0 error:NULL];
        for (NSURL *fileName in subpaths) {
            if (IsPathDirectory(manager, fileName.path)) {
                continue;
            } else if ([[fileName pathExtension] isEqualToString:@"snd"]) {
                ResID thePictResourceNumber = [[[fileName lastPathComponent] stringByDeletingPathExtension] intValue];
                Resource *theResource;
                
                if (thePictResourceNumber < 128) {
                    continue;
                }
                if ([maraResources resourceOfType:@"snd " index:thePictResourceNumber load:NO] != nil) {
                    continue;
                }
                
                NSError *err;
                NSData *sndData = [[NSData alloc] initWithContentsOfURL:fileName options:0 error:&err];
                if (!sndData) {
                    NSLog(@"Snd read failed: %@", err);
                    continue;
                }

                theResource = [[Resource alloc] initWithID:thePictResourceNumber type:@"snd " name:@""];
                theResource.data = sndData;
                [maraResources addResource:theResource];
            } else if ([[fileName pathExtension] isEqualToString:@"aif"] || [[fileName pathExtension] isEqualToString:@"aiff"]) {
                //TODO: convert from AIFF
                NSLog(@"Skipping %@: AIFF conversion not implemented!", fileName.lastPathComponent);
                continue;
            }
        }
    }
    
    [maraResources saveToURL:fullPath oldFileURL:nil];

    return YES;
}

// ****************** Information ******************
#pragma mark - Information

- (id)dataObjectForLevelNameTable
{
    return scenarioData;
}

- (NSArray *)levelNames
{
    return [scenarioData levelFileNames];
}

@end
