def create_release_notes():
    import os
    path = os.path.dirname(os.path.abspath(__file__))
    changelog_filename = os.path.join(path, "../CHANGELOG.md")
    release_notes_filename = os.path.join(path, "../RELEASE_NOTES.md")

    with open(changelog_filename, "r") as changelog:
        with open(release_notes_filename, "w") as release_notes:
            started = False

            # Search top-most release notes
            while not started:
                line = changelog.readline()
                if not line:
                    break

                if line.startswith("## ["):
                    started = True

            while started:
                # reduce title indentation
                if line.startswith("##"):
                    line = line[1:]

                release_notes.write(line)

                line = changelog.readline()
                if not line or line.startswith("## ["):
                    break

if __name__ == "__main__":
    create_release_notes()