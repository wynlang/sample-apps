// Turn a title into a URL slug.
export function slug(title) {
  return title
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-|-$/g, "");
}

/* Kept for the old admin pages.
   Remove once /admin/v1 is gone. */
export function unslug(s) {
  return s.split("-").join(" ");
}
