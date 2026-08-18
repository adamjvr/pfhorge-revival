// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// FORMAT-3A: unified AppKit document routing.
// Included by Other Sources/main.m before NSApplicationMain.

#import <Cocoa/Cocoa.h>

#define PFHORGE_FORMAT_3A_UNIFIED_DOCUMENT_CONTROLLER 1

static NSString * const PFH3APfhorgeLevelType =
    @"com.xmission.dragons.pfhorge.level";
static NSString * const PFH3APfhorgeScenarioType =
    @"com.xmission.dragons.pfhorge.scenario";
static NSString * const PFH3AMarathonMapType =
    @"org.bungie.source.map";

static BOOL PFH3ALooksLikeLegacyPfhorgeLevel(NSData *data)
{
    if (data.length < 10U) {
        return NO;
    }

    const uint8_t *bytes = data.bytes;
    const uint16_t sig1 = ((uint16_t)bytes[2] << 8U) | bytes[3];
    const uint16_t sig2 = ((uint16_t)bytes[4] << 8U) | bytes[5];
    const uint32_t sig3 =
        ((uint32_t)bytes[6] << 24U) |
        ((uint32_t)bytes[7] << 16U) |
        ((uint32_t)bytes[8] << 8U) |
        (uint32_t)bytes[9];

    return sig1 == 26743U &&
           sig2 == 34521U &&
           sig3 == 42296737U;
}

static OSType PFH3AHFSTypeForURL(NSURL *url)
{
    NSDictionary *attributes =
        [[NSFileManager defaultManager]
            attributesOfItemAtPath:url.path
                             error:NULL];
    NSNumber *typeNumber = attributes[NSFileHFSTypeCode];
    return typeNumber != nil
        ? (OSType)typeNumber.unsignedIntValue
        : (OSType)0;
}

static BOOL PFH3AIsGenericDataDocumentType(NSString *type)
{
    if (type.length == 0U) {
        return NO;
    }

    // Extensionless classic Mac files commonly arrive from AppKit as a
    // generic data type.  That classification must not short-circuit
    // Pfhorge's Marathon resolver/parser, which is the real validator.
    return [type isEqualToString:@"data"] ||
           [type isEqualToString:@"public.data"];
}

@interface PfhorgeUnifiedDocumentController : NSDocumentController
@end

@implementation PfhorgeUnifiedDocumentController

- (void)beginOpenPanel:(NSOpenPanel *)openPanel
              forTypes:(NSArray<NSString *> *)inTypes
     completionHandler:(void (^)(NSInteger result))completionHandler
{
    (void)inTypes;

    openPanel.allowsOtherFileTypes = YES;
    openPanel.canChooseFiles = YES;
    openPanel.canChooseDirectories = NO;
    openPanel.allowsMultipleSelection = YES;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    openPanel.allowedFileTypes = nil;
    [super beginOpenPanel:openPanel
                 forTypes:nil
        completionHandler:completionHandler];
#pragma clang diagnostic pop
}

- (NSInteger)runModalOpenPanel:(NSOpenPanel *)openPanel
                      forTypes:(NSArray<NSString *> *)types
{
    (void)types;

    openPanel.allowsOtherFileTypes = YES;
    openPanel.canChooseFiles = YES;
    openPanel.canChooseDirectories = NO;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    openPanel.allowedFileTypes = nil;
    return [super runModalOpenPanel:openPanel
                          forTypes:nil];
#pragma clang diagnostic pop
}

- (NSDocument *)makeDocumentWithContentsOfURL:(NSURL *)url
                                       ofType:(NSString *)typeName
                                        error:(NSError * _Nullable *)outError
{
    if (![typeName isEqualToString:PFH3AMarathonMapType]) {
        return [super makeDocumentWithContentsOfURL:url
                                            ofType:typeName
                                             error:outError];
    }

    // File -> Open on Marathon is an import operation.  Do not use the normal
    // initWithContentsOfURL: path because that records the Marathon URL/type as
    // this document's backing identity and later Save/Close/Quit machinery can
    // try to write the foreign source format back to disk.
    NSError *localError = nil;
    NSDocument *document =
        [self makeUntitledDocumentOfType:PFH3APfhorgeLevelType
                                   error:&localError];

    if (document == nil) {
        if (outError != NULL) *outError = localError;
        return nil;
    }

    if (![document readFromURL:url
                        ofType:PFH3AMarathonMapType
                         error:&localError]) {
        if (outError != NULL) *outError = localError;
        return nil;
    }

    // The object was born as an untitled Pfhorge Native document.  The source
    // URL above is input/provenance only; it is not the document backing URL.
    NSString *sourceName =
        url.lastPathComponent.length ? url.lastPathComponent : @"Imported Level";
    document.displayName =
        [sourceName stringByAppendingPathExtension:@"pfhlev"];

    [document updateChangeCount:NSChangeDone];

    NSLog(@"FORMAT-3A detached Marathon import: source=%@ nativeType=%@ backingURL=%@ displayName=%@",
          url.path,
          document.fileType ?: @"(nil)",
          document.fileURL.path ?: @"(none)",
          document.displayName ?: @"(nil)");

    return document;
}

- (NSString *)typeForContentsOfURL:(NSURL *)url
                             error:(NSError * _Nullable *)outError
{
    NSString *extension = url.pathExtension.lowercaseString;

    if ([extension isEqualToString:@"pfhlev"]) {
        return PFH3APfhorgeLevelType;
    }
    if ([extension isEqualToString:@"sen"]) {
        return PFH3APfhorgeScenarioType;
    }

    static NSSet<NSString *> *marathonExtensions = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        marathonExtensions =
            [NSSet setWithArray:@[@"sce", @"scen", @"scea"]];
    });
    if ([marathonExtensions containsObject:extension]) {
        return PFH3AMarathonMapType;
    }

    OSType hfsType = PFH3AHFSTypeForURL(url);
    if (hfsType == (OSType)'PfhL') {
        return PFH3APfhorgeLevelType;
    }
    if (hfsType == (OSType)'PfhS') {
        return PFH3APfhorgeScenarioType;
    }
    if (hfsType == (OSType)'sce2') {
        return PFH3AMarathonMapType;
    }

    NSData *data =
        [NSData dataWithContentsOfURL:url
                              options:NSDataReadingMappedIfSafe
                                error:NULL];
    if (PFH3ALooksLikeLegacyPfhorgeLevel(data)) {
        return PFH3APfhorgeLevelType;
    }

    NSError *normalError = nil;
    NSString *normalType =
        [super typeForContentsOfURL:url
                             error:&normalError];
    // Keep useful AppKit classifications, but ignore its generic-data
    // fallback so extensionless Marathon maps reach the importer.
    if (normalType.length > 0U &&
        !PFH3AIsGenericDataDocumentType(normalType)) {
        return normalType;
    }

    BOOL isDirectory = NO;
    BOOL exists =
        [[NSFileManager defaultManager]
            fileExistsAtPath:url.path
                 isDirectory:&isDirectory];

    if (exists && !isDirectory) {
        // The filename filter is not the source-format validator.
        // The Marathon resolver/parser gets the final say.
        NSLog(@"FORMAT-3A unified open: routing generic type %@ as Marathon candidate: %@",
              normalType.length > 0U ? normalType : @"(none)",
              url.path);
        return PFH3AMarathonMapType;
    }

    if (outError != NULL) {
        *outError = normalError ?: [NSError
            errorWithDomain:NSCocoaErrorDomain
                       code:NSFileReadUnknownError
                   userInfo:@{
                       NSURLErrorKey: url,
                       NSLocalizedFailureReasonErrorKey:
                           @"Pfhorge could not determine a supported document type."
                   }];
    }
    return nil;
}

@end

static PfhorgeUnifiedDocumentController *
    PFH3ASharedDocumentController = nil;

static void PfhorgeInstallUnifiedDocumentController(void)
{
    if (PFH3ASharedDocumentController == nil) {
        // AppKit: the first NSDocumentController instance becomes shared.
        PFH3ASharedDocumentController =
            [[PfhorgeUnifiedDocumentController alloc] init];
    }
}
