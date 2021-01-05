# cmake/Hunter/passwords.cmake

hunter_upload_password(
    # REPO_OWNER + REPO = https://github.com/forexample/hunter-cache
    REPO_OWNER "koinos"
    REPO "hunter-cache"

    # USERNAME = https://github.com/ingenue
    USERNAME "koinos-ci"

    # PASSWORD = GitHub token saved as a secure environment variable
    PASSWORD "$ENV{GITHUB_USER_PASSWORD}"
)
