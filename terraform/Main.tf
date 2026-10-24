terraform {
  required_version = ">= 1.5"

  required_providers {
    github = {
      source  = "integrations/github"
      version = "~> 6.0"
    }
  }
}

provider "github" {
  owner = "horsenuggets"
  # Authenticates via the GITHUB_TOKEN environment variable.
}

# Adopt the existing repository instead of trying to create it.
import {
  to = module.repo.github_repository.repo
  id = "luau-lsp"
}

module "repo" {
  source     = "../submodules/luau-cicd/Terraform/Modules/LuauRepo"
  repository = "luau-lsp"

  main_checks = [
    "Build Server (macos-latest)",
    "Build Server (ubuntu-22.04)",
    "Build Server (windows-latest)",
    "Lint Extension",
    "Run Tests (macos-latest)",
    "Run Tests (ubuntu-22.04)",
    "Run Tests (windows-latest)",
    "Run selene",
    "Run stylua",
    "Test Extension (macos-latest)",
    "Test Extension (ubuntu-22.04)",
    "Test Extension (windows-latest)",
    "Types Smoketest",
    "Verify and Build Roblox Studio Plugin",
  ]

  release_checks = [
    "Build test - Extension",
    "Build test - Linux aarch64",
    "Build test - Linux x86_64",
    "Build test - Windows",
    "Build test - macOS",
    "Validate PR title",
    "Validate version",
    "Verify diff matches main",
  ]
}
