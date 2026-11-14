"""Provider-neutral identifiers and versioned local interchange contract."""
import re

SCHEMA_VERSION = 1


def validate_client(value):
    if not isinstance(value, str) or not re.fullmatch(r'[a-z][a-z0-9_-]{0,47}', value):
        raise ValueError('Client must be a safe provider-neutral identifier')
    return value
