/**
 * Simple i18n library for Solaris
 * Default language: Estonian (et)
 */

class I18n {
    constructor(defaultLanguage = 'et') {
        // Try to load saved language preference from localStorage
        const savedLanguage = localStorage.getItem('solaris_language');
        this.currentLanguage = savedLanguage || defaultLanguage;
        this.translations = {};
        this.fallbackLanguage = 'en';
    }

    /**
     * Load translation file
     */
    async loadLanguage(language) {
        const base = new URL('.', import.meta.url);
        const langUrl = new URL(`${language}.json`, base);
        
        try {
            const response = await fetch(langUrl);
            if (!response.ok) {
                throw new Error(`Failed to load language file: ${language}`);
            }
            this.translations[language] = await response.json();
            return true;
        } catch (error) {
            console.error(`Error loading language ${language}:`, error);
            return false;
        }
    }

    /**
     * Initialize i18n with a specific language
     */
    async init(language = null) {
        if (language) {
            this.currentLanguage = language;
        }
        
        // Load current language
        await this.loadLanguage(this.currentLanguage);
        
        // Load fallback language if different
        if (this.currentLanguage !== this.fallbackLanguage) {
            await this.loadLanguage(this.fallbackLanguage);
        }
        
        // Update html lang attribute after language is loaded
        document.documentElement.lang = this.currentLanguage;
        
        // Apply translations to the page
        this.applyTranslations();
    }

    /**
     * Get translation for a key
     * Key format: "section.key" or "section.subsection.key"
     */
    t(key, replacements = {}) {
        const keys = key.split('.');
        let value = this.translations[this.currentLanguage];
        
        // Navigate through the nested object
        for (const k of keys) {
            if (value && typeof value === 'object' && k in value) {
                value = value[k];
            } else {
                // Fallback to English if key not found
                value = this.translations[this.fallbackLanguage];
                for (const k2 of keys) {
                    if (value && typeof value === 'object' && k2 in value) {
                        value = value[k2];
                    } else {
                        value = key; // Return key if not found
                        break;
                    }
                }
                break;
            }
        }
        
        // Replace placeholders like {name}, {count}, etc.
        if (typeof value === 'string') {
            for (const [placeholder, replacement] of Object.entries(replacements)) {
                value = value.replace(new RegExp(`\\{${placeholder}\\}`, 'g'), replacement);
            }
        }
        
        return value;
    }

    /**
     * Apply translations to elements with data-i18n attribute
     */
    applyTranslations() {
        // Translate elements with data-i18n attribute
        document.querySelectorAll('[data-i18n]').forEach(element => {
            const key = element.getAttribute('data-i18n');
            const translation = this.t(key);
            
            // Check if we should translate a specific attribute
            const attr = element.getAttribute('data-i18n-attr');
            if (attr) {
                element.setAttribute(attr, translation);
            } else {
                // Translate text content
                element.textContent = translation;
            }
        });

        // Translate placeholders
        document.querySelectorAll('[data-i18n-placeholder]').forEach(element => {
            const key = element.getAttribute('data-i18n-placeholder');
            const translation = this.t(key);
            element.placeholder = translation;
        });

        // Update page title if it has data-i18n-title
        const titleElement = document.querySelector('[data-i18n-title]');
        if (titleElement) {
            const key = titleElement.getAttribute('data-i18n-title');
            document.title = this.t(key);
        }
    }

    /**
     * Change the current language and reapply translations
     */
    async changeLanguage(language) {
        if (!this.translations[language]) {
            await this.loadLanguage(language);
        }
        this.currentLanguage = language;
        // Save language preference to localStorage
        localStorage.setItem('solaris_language', language);
        // Update html lang attribute
        document.documentElement.lang = language;
        this.applyTranslations();
    }

    /**
     * Get current language
     */
    getCurrentLanguage() {
        return this.currentLanguage;
    }
}

// Create global instance with Estonian as default
window.i18n = new I18n('et');
