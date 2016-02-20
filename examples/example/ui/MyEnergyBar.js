//  myEnergyBar.js
//  examples/ui
//
//  Created by Eric Levin on 1/4/15
//  Copyright 2015 High Fidelity, Inc.
//
//  This script adds an energy bar overlay which displays the amount of energy a user has left for grabbing and moving objects
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
Script.include("../../libraries/utils.js");
var energyColor = {red: 0, green: 200, blue: 0};
var lowEnergyColor = {red: 255, green: 0, blue: 0};
var totalWidth = 200;
var paddingRight = 50;
var xPosition = Window.innerWidth - totalWidth - paddingRight;
var lowEnergyThreshold = 0.3;
var currentEnergy = 1.0;
var isGrabbing = false;
var refractoryPeriod = 2000;

var background = Overlays.addOverlay("text", {
    x: xPosition,
    y: 20,
    width: totalWidth,
    height: 10,
    backgroundColor: {red: 255, green: 0, blue: 0}
})

var bar = Overlays.addOverlay("text", {
    x: xPosition,
    y: 20,
    width: totalWidth,
    height: 10,
    backgroundColor: energyColor
});


// Takes an energy value between 0 and 1 and sets energy bar width appropriately
function updateDisplay() {
    var barWidth = totalWidth * MyAvatar.energy;
    var color = MyAvatar.energy <= lowEnergyThreshold ? lowEnergyColor: energyColor;
    Overlays.editOverlay(bar, { width: barWidth, backgroundColor: color});
}

function cleanup() {
    Entities.setEnergyCostMultiplier(0.0);
    Overlays.deleteOverlay(background);
    Overlays.deleteOverlay(bar);
}

function energyChanged(newValue) {
    Entities.currentAvatarEnergy = newValue;
    updateDisplay();
}

function debitAvatarEnergy(value) {
    MyAvatar.energy = MyAvatar.energy - value;
}

Entities.setEnergyCostMultiplier(1.0);
print("Energy-mass multiplier = " + Entities.getEnergyCostMultiplier);
Entities.debitEnergySource.connect(debitAvatarEnergy);
MyAvatar.energyChanged.connect(energyChanged);
Script.scriptEnding.connect(cleanup);
