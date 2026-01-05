#!/usr/bin/env node

const https = require("https");
const fs = require("fs");
const path = require("path");
const { execSync } = require("child_process");
const zlib = require("zlib");

const PACKAGE_VERSION = require("../package.json").version;
const GITHUB_REPO = "rtorr/nah";

const PLATFORM_MAP = {
  "darwin-x64": "darwin-x64",
  "darwin-arm64": "darwin-arm64",
  "linux-x64": "linux-x64",
  "linux-arm64": "linux-arm64",
};

function getPlatformKey() {
  const platform = process.platform;
  const arch = process.arch;
  return `${platform}-${arch}`;
}

function getBinaryName() {
  return process.platform === "win32" ? "nah.exe" : "nah";
}

function getDownloadUrl(platformKey) {
  const binaryName = getBinaryName();
  return `https://github.com/${GITHUB_REPO}/releases/download/v${PACKAGE_VERSION}/nah-${platformKey}.tar.gz`;
}

function downloadFile(url) {
  return new Promise((resolve, reject) => {
    const follow = (url, redirects = 0) => {
      if (redirects > 5) {
        reject(new Error("Too many redirects"));
        return;
      }

      https
        .get(url, (response) => {
          if (
            response.statusCode >= 300 &&
            response.statusCode < 400 &&
            response.headers.location
          ) {
            follow(response.headers.location, redirects + 1);
            return;
          }

          if (response.statusCode !== 200) {
            reject(
              new Error(`Failed to download: HTTP ${response.statusCode}`),
            );
            return;
          }

          const chunks = [];
          response.on("data", (chunk) => chunks.push(chunk));
          response.on("end", () => resolve(Buffer.concat(chunks)));
          response.on("error", reject);
        })
        .on("error", reject);
    };

    follow(url);
  });
}

function extractTarGz(buffer, destDir) {
  return new Promise((resolve, reject) => {
    const gunzip = zlib.createGunzip();
    const chunks = [];

    gunzip.on("data", (chunk) => chunks.push(chunk));
    gunzip.on("end", () => {
      const tarData = Buffer.concat(chunks);
      extractTar(tarData, destDir);
      resolve();
    });
    gunzip.on("error", reject);

    gunzip.end(buffer);
  });
}

function extractTar(tarData, destDir) {
  // Simple tar extraction - handles standard POSIX tar format
  let offset = 0;

  while (offset < tarData.length) {
    // Read header (512 bytes)
    const header = tarData.slice(offset, offset + 512);
    offset += 512;

    // Check for end of archive (two zero blocks)
    if (header.every((b) => b === 0)) {
      break;
    }

    // Parse header
    const name = header
      .slice(0, 100)
      .toString("utf8")
      .replace(/\0/g, "")
      .trim();
    const sizeOctal = header
      .slice(124, 136)
      .toString("utf8")
      .replace(/\0/g, "")
      .trim();
    const size = parseInt(sizeOctal, 8) || 0;
    const typeFlag = header[156];

    if (!name) {
      break;
    }

    // Calculate padded size (512-byte blocks)
    const paddedSize = Math.ceil(size / 512) * 512;

    // Type: '0' or '\0' = regular file, '5' = directory
    if (typeFlag === 48 || typeFlag === 0) {
      // Regular file
      const filePath = path.join(destDir, path.basename(name));
      const fileData = tarData.slice(offset, offset + size);
      fs.writeFileSync(filePath, fileData);

      // Make executable if it's the nah binary
      if (path.basename(name) === "nah") {
        fs.chmodSync(filePath, 0o755);
      }
    }

    offset += paddedSize;
  }
}

async function main() {
  const platformKey = getPlatformKey();
  const supportedPlatform = PLATFORM_MAP[platformKey];

  if (!supportedPlatform) {
    console.error(`Unsupported platform: ${platformKey}`);
    console.error(
      `Supported platforms: ${Object.keys(PLATFORM_MAP).join(", ")}`,
    );
    process.exit(1);
  }

  const binDir = path.join(__dirname, "..", "npm-bin");
  const binaryPath = path.join(binDir, getBinaryName());

  // Check if binary already exists
  if (fs.existsSync(binaryPath)) {
    console.log("nah binary already installed");
    return;
  }

  const url = getDownloadUrl(supportedPlatform);
  console.log(`Downloading nah v${PACKAGE_VERSION} for ${platformKey}...`);

  try {
    const buffer = await downloadFile(url);
    console.log("Extracting...");

    // Ensure bin directory exists
    if (!fs.existsSync(binDir)) {
      fs.mkdirSync(binDir, { recursive: true });
    }

    await extractTarGz(buffer, binDir);

    // Verify the binary exists
    if (!fs.existsSync(binaryPath)) {
      throw new Error("Binary not found after extraction");
    }

    console.log(`nah v${PACKAGE_VERSION} installed successfully`);
  } catch (error) {
    console.error(`Failed to install nah: ${error.message}`);
    console.error("");
    console.error("You can try installing manually:");
    console.error(`  1. Download from: ${url}`);
    console.error(`  2. Extract to: ${binDir}`);
    process.exit(1);
  }
}

main();
