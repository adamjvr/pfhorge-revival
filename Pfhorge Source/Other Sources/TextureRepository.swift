// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2023 C.W. Betts
// TEX-1A / TEX-1A.1 modifications Copyright (C) 2026 Adam Vadala-Roth

import Cocoa

@objcMembers
final class TextureRepository: NSObject {
    @MainActor @objc(sharedTextureRepository)
    static let shared = TextureRepository()

    private var wallCollections: [Int32: [NSImage]] = [:]
    private var landscapeCollections: [Int32: [NSImage]] = [:]
    private var loadErrors: [Int32: String] = [:]
    private var loadedShapesURL: URL?
    private(set) var generation: UInt64 = 0

    private override init() {
        super.init()
    }

    private var selectedShapesURL: URL? {
        UserDefaults.standard.url(forKey: VMShapesPath)?.standardizedFileURL
    }

    private func clearCatalog() {
        wallCollections.removeAll(keepingCapacity: true)
        landscapeCollections.removeAll(keepingCapacity: true)
        loadErrors.removeAll(keepingCapacity: true)
        generation &+= 1
    }

    /// Invalidates stale arrays automatically when Content Manager selects a
    /// different Shapes source, even if a caller missed the notification.
    private func synchronizeSelectedShapes() -> URL? {
        let selected = selectedShapesURL
        if selected != loadedShapesURL {
            clearCatalog()
            loadedShapesURL = selected
        }
        return selected
    }

    private func normalizedCollection(_ collection: Int32) -> Int32? {
        switch collection {
        case 0...4, 10...13:
            return collection
        case 17...21, 27...30:
            return collection - 17
        default:
            return nil
        }
    }

    private func environmentCode(for normalized: Int32) -> LELevelEnvironmentCode? {
        switch normalized {
        case 0: return .water
        case 1: return .lava
        case 2: return .sewage
        case 3: return .jjaro
        case 4: return .pfhor
        case 10: return .landscape1
        case 11: return .landscape2
        case 12: return .landscape3
        case 13: return .landscape4
        default: return nil
        }
    }

    private func store(_ images: [NSImage], for normalized: Int32) {
        if (0...4).contains(normalized) {
            wallCollections[normalized] = images
        } else {
            landscapeCollections[normalized] = images
        }
        loadErrors.removeValue(forKey: normalized)
        generation &+= 1
    }

    private func cachedImages(for normalized: Int32) -> [NSImage]? {
        if (0...4).contains(normalized) {
            return wallCollections[normalized]
        }
        return landscapeCollections[normalized]
    }

    private func loadNormalizedCollection(_ normalized: Int32) throws -> [NSImage] {
        guard let shapesURL = synchronizeSelectedShapes() else {
            throw CocoaError(.fileNoSuchFile)
        }
        guard let environment = environmentCode(for: normalized) else {
            throw CocoaError(.fileReadUnsupportedScheme)
        }

        do {
            let images = try getAllTextures(
                collection: environment,
                colorTable: 0,
                shapesPath: shapesURL
            )
            store(images, for: normalized)
            return images
        } catch {
            loadErrors[normalized] = error.localizedDescription
            throw error
        }
    }

    func loadTextureSet(_ textureSet: Int32) throws {
        guard let normalized = normalizedCollection(textureSet) else {
            return
        }
        _ = try loadNormalizedCollection(normalized)
    }

    /// Reloads every classic wall and landscape collection from the selected
    /// Shapes file. Individual lookups are also lazy, so a missing collection
    /// cannot prevent valid wall collections from rendering.
    func loadAllTextures() {
        loadedShapesURL = selectedShapesURL
        clearCatalog()
        guard loadedShapesURL != nil else {
            NSLog("*** No valid shapes file! ***")
            return
        }

        let collections: [Int32] = [0, 1, 2, 3, 4, 10, 11, 12, 13]
        for collection in collections {
            do {
                _ = try loadNormalizedCollection(collection)
            } catch {
                NSLog(
                    "*** Texture collection %d failed: %@ ***",
                    collection,
                    error.localizedDescription
                )
            }
        }
        NSLog("*** Done Loading Textures ***")
    }

    @objc(reloadClassicTextures)
    func reloadClassicTextures() {
        loadAllTextures()
    }

    private func imageArray(forCollection collection: Int32) -> [NSImage]? {
        guard let normalized = normalizedCollection(collection) else {
            return nil
        }
        _ = synchronizeSelectedShapes()
        if let cached = cachedImages(for: normalized) {
            return cached
        }
        return try? loadNormalizedCollection(normalized)
    }

    func textureCollection(_ collection: Int32) -> [NSImage]? {
        imageArray(forCollection: collection)
    }

    @objc(textureForCollection:bitmap:)
    func texture(forCollection collection: Int32, bitmap: Int32) -> NSImage? {
        guard bitmap >= 0,
              let images = imageArray(forCollection: collection) else {
            return nil
        }
        let index = Int(bitmap)
        guard images.indices.contains(index) else {
            return nil
        }
        return images[index]
    }

    @objc(texturesForEnvironment:)
    func textures(for collection: LELevelEnvironmentCode) -> [NSImage]? {
        imageArray(forCollection: Int32(collection.rawValue))
    }

    /// Objective-C audit bridge used by Content > Audit Active Map Textures.
    @objc(classicTextureAuditSummary)
    func classicTextureAuditSummary() -> [String: Any] {
        _ = synchronizeSelectedShapes()
        var counts: [String: Int] = [:]
        for collection in Int32(0)...Int32(4) {
            counts[String(collection)] = wallCollections[collection]?.count ?? 0
        }
        for collection in [Int32(10), 11, 12, 13] {
            counts[String(collection)] = landscapeCollections[collection]?.count ?? 0
        }
        return [
            "shapesPath": loadedShapesURL?.path ?? "",
            "generation": generation,
            "collectionCounts": counts,
            "errors": loadErrors.mapKeys { String($0) },
        ]
    }
}

private extension Dictionary where Key == Int32, Value == String {
    func mapKeys(_ transform: (Int32) -> String) -> [String: String] {
        Dictionary<String, String>(
            uniqueKeysWithValues: map { (transform($0.key), $0.value) }
        )
    }
}

private func getAllTextures(
    collection theCollection: LELevelEnvironmentCode,
    colorTable theColorTable: Int32,
    shapesPath: URL
) throws -> [NSImage] {
    var error: NSError?
    guard let images = __getAllTexturesOfWithError(
        Int32(theCollection.rawValue),
        theColorTable,
        shapesPath,
        &error
    ) else {
        if let error {
            throw error
        }
        throw CocoaError(
            .fileReadUnknown,
            userInfo: [NSURLErrorKey: shapesPath]
        )
    }
    return images
}
