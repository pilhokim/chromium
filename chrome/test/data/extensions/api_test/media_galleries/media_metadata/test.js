// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

function RunMetadataTest(filename, callOptions, verifyMetadataFunction) {
  function getMediaFileSystemsCallback(results) {
    chrome.test.assertEq(1, results.length);
    var gallery = results[0];
    gallery.root.getFile(filename, {create: false}, verifyFileEntry,
      chrome.test.fail);
  }

  function verifyFileEntry(fileEntry) {
    fileEntry.file(verifyFile, chrome.test.fail)
  }

  function verifyFile(file) {
    mediaGalleries.getMetadata(file, callOptions, verifyMetadataFunction);
  }

  mediaGalleries.getMediaFileSystems(getMediaFileSystemsCallback);
}

function ImageMIMETypeOnlyTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("image/jpeg", metadata.mimeType);
    chrome.test.succeed();
  }

  RunMetadataTest("test.jpg", {metadataType: 'mimeTypeOnly'}, verifyMetadata);
}

function MP3MIMETypeOnlyTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("audio/mpeg", metadata.mimeType);
    chrome.test.assertEq(undefined, metadata.title);
    chrome.test.succeed();
  }

  RunMetadataTest("id3_png_test.mp3", {metadataType: 'mimeTypeOnly'},
                  verifyMetadata);
}

function MP3TagsTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("audio/mpeg", metadata.mimeType);
    chrome.test.assertEq("Airbag", metadata.title);
    chrome.test.assertEq("Radiohead", metadata.artist);
    chrome.test.assertEq("OK Computer", metadata.album);
    chrome.test.assertEq(1, metadata.track);
    chrome.test.assertEq("Alternative", metadata.genre);
    chrome.test.succeed();
  }

  return RunMetadataTest("id3_png_test.mp3", {}, verifyMetadata);
}

function RotatedVideoTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("video/mp4", metadata.mimeType);
    chrome.test.assertEq(90, metadata.rotation);
    chrome.test.succeed();
  }

  return RunMetadataTest("90rotation.mp4", {}, verifyMetadata);
}

chrome.test.getConfig(function(config) {
  var customArg = JSON.parse(config.customArg);
  var useProprietaryCodecs = customArg[0];

  // Should still be able to sniff MP3 MIME type without proprietary codecs.
  var testsToRun = [
    ImageMIMETypeOnlyTest
  ];

  if (useProprietaryCodecs) {
    testsToRun = testsToRun.concat([
      MP3MIMETypeOnlyTest,
      MP3TagsTest,
      RotatedVideoTest
    ]);
  }

  chrome.test.runTests(testsToRun);
});
