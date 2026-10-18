import { defineConfig } from "astro/config";

export default defineConfig({
  site: "https://bemarketing-hosting.github.io",
  base: process.env.BASE_PATH || "/stick-s3-deck-clicker/",
  output: "static",
});
