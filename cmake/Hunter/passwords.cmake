# cmake/Hunter/passwords.cmake

hunter_upload_password(
    REPO_OWNER "koinos"
    REPO "hunter-cache"
    USERNAME "koinos-ci"
    PASSWORD "$ENV{GITHUB_USER_PASSWORD}"
)
