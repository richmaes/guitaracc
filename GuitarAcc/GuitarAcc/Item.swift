//
//  Item.swift
//  GuitarAcc
//
//  Created by Rich Maes on 6/13/26.
//

import Foundation
import SwiftData

@Model
final class Item {
    var timestamp: Date
    
    init(timestamp: Date) {
        self.timestamp = timestamp
    }
}
