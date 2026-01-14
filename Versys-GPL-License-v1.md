# Versys General Public License (VGPL) v1.0
## For Software and Open Source Projects

**Effective Date:** January 14, 2026

---

## PREAMBLE

This Versys General Public License ("VGPL") is designed to protect open source software while enabling research, development, and innovation. It grants broad freedoms for non-commercial use while establishing clear commercial limitations and attribution requirements.

The license balances:
- **Freedom**: Full R&D rights for academic and research purposes
- **Openness**: Requirement to share source code modifications
- **Attribution**: Recognition of original creators and dependencies
- **Protection**: Safeguards against commercial exploitation without significant innovation

---

## TERMS AND CONDITIONS

### 1. DEFINITIONS

- **"Software"** or **"Project"**: The original work distributed under this license, including source code, documentation, and compiled binaries.
- **"Derivative Work"** or **"Modified Version"**: Any version, adaptation, or modification of the Software.
- **"Commercial Use"**: Any use intended to generate revenue, profit, or commercial advantage, whether directly or indirectly.
- **"Research and Development (R&D)"**: Non-commercial use for educational, experimental, or improvement purposes, including academic research and internal prototyping.
- **"Substantially Similar"**: Code similarity exceeding 80% of the original in structure, logic, or functionality.
- **"Significant Modification"**: Architectural, code, or structural changes exceeding 20% of the original design.
- **"You"** or **"Licensee"**: Any person or entity exercising permissions granted by this license.
- **"Distribute"** or **"Distribution"**: Making the Software available to third parties through any means.

---

## 2. GRANT OF RIGHTS

### 2.1 Non-Commercial Use

You are freely granted the right to:

- **Use**: Execute, run, and operate the Software for any non-commercial purpose.
- **Copy**: Reproduce the Software in its original form for R&D purposes.
- **Modify**: Create Derivative Works for R&D, testing, and improvement.
- **Study**: Examine source code to understand functionality and design.
- **Research**: Use the Software as a basis for academic or scientific research.
- **Internal Deployment**: Use the Software within your organization for non-commercial purposes.

These rights require **no permission** and are **royalty-free**.

### 2.2 Partial Code and Code Segments

You may:

- Extract and study individual functions, libraries, or code segments for educational purposes.
- Adapt code segments into your own projects for **R&D purposes only**.
- Use compiled or object code as dependencies in non-commercial applications.

**Restriction**: Partial code excerpts integrated into Derivative Works for commercial purposes must comply with Section 3 (Commercial Use Restrictions).

### 2.3 Compiled and Binary Use

You may:

- Use the compiled Software, binaries, or object files without modification for any non-commercial purpose.
- Link the compiled Software as a dependency in your applications (non-commercial).
- Redistribute unmodified compiled versions if original source is clearly attributed and available.

---

## 3. COMMERCIAL USE RESTRICTIONS

### 3.1 Commercial Prohibition Without Modification

You **MAY NOT** use the Software for commercial purposes if:

- Your Derivative Work is **substantially similar** (>80% similarity) to the original Software.
- Your application **directly incorporates** unmodified code from the Software without modification.
- You are generating revenue or commercial advantage using primarily the original Software.

**Penalty**: Unauthorized commercial use requires either:
1. **Immediate cessation** of commercial distribution, OR
2. **Negotiation** of a separate commercial license with the original author.

### 3.2 Commercial Use With Significant Modification

Commercial use is **permitted** if your Derivative Work meets **ALL** of the following criteria:

- **Architectural Differences**: The system architecture differs by 20% or more from the original.
- **Code Changes**: Substantive modifications, additions, or redesigns constitute 20% or more of the codebase.
- **Functional Enhancements**: The Derivative Work provides demonstrably different functionality or features.
- **Clear Attribution**: The original project is prominently credited in documentation, licensing, and user-facing materials.

**Example of Compliant Commercial Use**:
- Original: Task management library
- Derivative: Enterprise project management platform incorporating the library with custom API, UI, authentication, analytics, and business logic (>20% custom code)
- Status: ✓ **Permitted** if attributed

**Example of Non-Compliant Commercial Use**:
- Original: Task management library
- Derivative: Same library repackaged with minimal changes and sold as a premium service
- Status: ✗ **Prohibited** (does not meet 20% modification threshold)

### 3.3 Partial Code in Commercial Projects

When integrating portions of the Software into commercial products:

- **Attribution Required**: You must identify which code segments originate from this Project and include this license or a summary.
- **Dependency Listing**: This project must be listed as a dependency with clear version and link.
- **No Concealment**: You must not obscure or hide the original project's contribution.
- **Source Availability**: The portion of code derived from the Software must be traceable to its original source.

**Violation Example**: Including 40% of the original code in a commercial product without attribution or listing it as a dependency is **strictly prohibited**.

---

## 4. COPYLEFT OBLIGATION (Share-Alike)

### 4.1 Source Code Distribution

If you distribute any Derivative Work (modified or unmodified), you **must**:

- **Provide Source Code**: Make the complete, corresponding source code available.
- **Specify License**: Clearly indicate that the Derivative Work is licensed under VGPL.
- **Include Original License**: Provide a copy of this VGPL license with distributions.

### 4.2 Exception for Proprietary Linking

You may **link** the Software as an **unmodified dependency** in proprietary applications without disclosing your source code, provided:

- You distribute the original Software's source separately (or provide a link to obtain it).
- You clearly list this Software as a dependency.
- You do not modify the Software's source code.

---

## 5. ATTRIBUTION AND DEPENDENCY REQUIREMENTS

### 5.1 Source Code Attribution

Every source file derived from or containing code from the Software must include:

```
This code contains portions of [Project Name]
Licensed under Versys General Public License (VGPL) v1.0
Original Project: [URL/Link]
Original Author: [Name/Organization]
```

### 5.2 Documentation and README

All distributions must include:

- **Acknowledgment**: Clear statement that the Software incorporates or is based on the original Project.
- **Dependency List**: List this Project with version, URL, and license (VGPL v1.0).
- **License Copy**: Include or link to the full VGPL license text.

**Minimum README Entry**:
```
## Dependencies

This project includes code from [Project Name] (VGPL v1.0)
- Original Project: [URL]
- License: [VGPL v1.0](link-to-license)
- Modifications: [Brief description if applicable]
```

### 5.3 Commercial Distribution

Commercial products must include attribution in:

- **Software Documentation**
- **End-User License Agreement**
- **About/Credits Section**
- **Source Code Headers** (if distributed)

---

## 6. NO WARRANTY

### 6.1 Disclaimer of Warranties

**THE SOFTWARE IS PROVIDED "AS IS"** WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO:

- **Fitness for a Particular Purpose**
- **Merchantability**
- **Non-Infringement of Third-Party Rights**
- **Correctness, Accuracy, or Completeness**
- **Performance or Reliability**

### 6.2 Limitation of Liability

In **no event** shall the original author or copyright holder be liable for:

- **Direct, Indirect, Incidental, Special, Consequential, or Punitive Damages**
- **Loss of Profits, Revenue, Data, or Business Opportunity**
- **System Failures, Downtime, or Service Interruption**
- **Security Breaches or Unauthorized Access**
- **Any Other Damages** arising from the use or inability to use the Software

This includes damages caused by:
- Bugs, errors, or defects
- Security vulnerabilities
- Data corruption or loss
- Operational failures
- Third-party interference

### 6.3 Your Responsibility

By using this Software, you assume **full responsibility** for:

- **Testing and Validation**: You must independently test the Software for your use case.
- **Risk Assessment**: You understand the risks and limitations before deployment.
- **Appropriate Use**: You will use the Software in a safe and responsible manner.
- **Backup and Recovery**: You maintain backups and disaster recovery procedures.

---

## 7. TERMINATION

### 7.1 Automatic Termination

Your rights under this license automatically terminate if you:

- **Breach Terms**: Violate any provision of this license.
- **Unauthorized Commercial Use**: Engage in prohibited commercial use without modification.
- **Fail Attribution**: Fail to maintain required attribution.
- **Ignore Warnings**: Continue violation after written notice to cease.

### 7.2 Reinstatement

Terminated rights may be reinstated if you:

1. **Cease Violation**: Stop the infringing activity.
2. **Correct Attribution**: Add proper attribution and dependency acknowledgment.
3. **Comply Prospectively**: Demonstrate commitment to future compliance.

---

## 8. ENFORCEMENT

### 8.1 Legal Remedies

Violations of this license may result in:

- **Injunctive Relief**: Court orders to cease infringing activities.
- **Damages**: Recovery of losses caused by license violation.
- **Attorney Fees**: Reimbursement of legal costs.
- **License Revocation**: Permanent loss of rights to use the Software.

### 8.2 Right to Enforce

The original author and copyright holder retain the exclusive right to:

- **Pursue Legal Action**: Against unauthorized commercial use or material violations.
- **Negotiate Settlements**: Offering alternative commercial licensing arrangements.
- **Modify Terms**: For future versions while respecting existing licenses.

---

## 9. MODIFICATION AND VERSIONING

### 9.1 License Updates

The original author may publish updated versions of VGPL with updated version numbers (e.g., VGPL v2.0).

**Automatic Version Upgrade**: When a new version of VGPL is released, it **automatically takes effect** for all Software distributed under this license, without requiring you to manually update license files or version references in your project. This ensures consistent licensing across all distributions and derivative works.

- **No Manual Update Required**: You do not need to modify license files, headers, or documentation when new versions are released.
- **Automatic Enforcement**: All new distributions, modifications, and uses automatically fall under the latest published version unless you explicitly opt for an earlier version.
- **Check License Registry**: Current and historical VGPL versions are published and maintained at: **https://versys-research.com/open-collaborations/license/releases/**
- **Backward Compatibility**: Existing works licensed under prior versions remain valid and enforceable.
- **Your Choice (Optional)**: If preferred, you may explicitly declare you are continuing under an earlier version by stating the specific version number in your LICENSE file.
- **Version Verification**: To verify which version of VGPL applies to your project, check the release registry at the above URL for the publication date.

### 9.2 Project Modifications

You may modify the Software for **R&D or commercial purposes** (if meeting criteria in Section 3), but:

- **You Cannot**: Change the license terms for existing distributions without author consent.
- **You Must**: Specify modifications clearly in version numbering or documentation.

---

## 10. COMPATIBILITY AND INTEGRATION

### 10.1 Compatible Licenses

This Software may be integrated with projects licensed under:

- **MIT License**: Permissive, commercial-friendly
- **Apache 2.0**: Permissive with patent protections
- **GPL-Compatible Licenses**: Approved open source licenses

### 10.2 Incompatible Combinations

You **cannot** relicense the Software under:

- **Proprietary Licenses**: Closed-source commercial terms
- **Restrictive Terms**: That contradict VGPL's copyleft obligations
- **Non-Attribution Terms**: That remove original attribution

If combining with incompatible licenses, you must:
- Keep this Software as a **separate dependency**
- Provide original license and source
- Clearly separate licensed components

---

## 11. SPECIAL PROVISIONS

### 11.1 Academic and Educational Use

Educational institutions and researchers may:

- Use the Software for teaching and coursework without restriction.
- Publish research findings based on the Software.
- Create educational Derivative Works (with attribution).
- Distribute coursework materials using the Software (for students only).

### 11.2 Non-Profit Organizations

Non-profit entities may:

- Use the Software for charitable missions without commercial restrictions.
- Distribute Derivative Works for non-profit purposes.
- Maintain attribution requirements.

### 11.3 Government and Public Sector

Government agencies may:

- Use the Software for public services.
- Adapt for specific government needs (with attribution).
- Distribute to other government entities.

---

## 12. DISPUTE RESOLUTION

### 12.1 Good-Faith Negotiation

If disputes arise, parties agree to:

1. **Direct Communication**: Contact the original author to resolve concerns.
2. **Mediation**: Use informal dispute resolution before litigation.
3. **Documentation**: Keep records of communications.

### 12.2 Governing Law

This license is governed by the laws of **Vietnam** (or specify your jurisdiction).

- **Disputes**: Resolved in courts of Ho Chi Minh City, Vietnam.
- **International Use**: Interpreted consistently across jurisdictions.

---

## 13. ENTIRE AGREEMENT

This VGPL v1.0 constitutes the complete agreement between you and the copyright holder regarding the Software, superseding all prior understandings.

---

## 14. SEVERABILITY

If any provision is found invalid or unenforceable:

- **Remaining Terms**: All other provisions remain in effect.
- **Modification**: The invalid provision is modified to the minimum extent necessary to make it valid.
- **Intent**: The overall intent of the license is preserved.

---

## APPENDIX A: COMMERCIAL LICENSE NEGOTIATION TEMPLATE

For projects not meeting the 20% modification threshold but requiring commercial use, contact the original author:

**Template Email**:
```
Subject: VGPL Commercial License Request

Dear [Original Author],

I wish to use [Project Name] in a commercial product: [Description]

Current modifications: [Percentage and description]

Proposed arrangement: [License terms or revenue-sharing model]

Contact: [Your email and organization]

Regards,
[Your name]
```

---

## APPENDIX B: MODIFICATION THRESHOLD VERIFICATION CHECKLIST

Use this checklist to verify your Derivative Work meets the 20% modification requirement:

- [ ] **Architecture Changes**: Redesigned system components (20%+ changed)
- [ ] **Code Additions**: New functionality not in original (20%+ new code)
- [ ] **Library Integration**: Different dependencies or frameworks integrated
- [ ] **Feature Enhancements**: Original features significantly extended
- [ ] **Performance Optimization**: Substantial refactoring for efficiency
- [ ] **Security Hardening**: Security mechanisms added beyond original
- [ ] **Scalability Improvements**: Modified for different scale/capacity
- [ ] **Platform Adaptations**: Ported to different OS/architecture significantly
- [ ] **API/Interface Changes**: User-facing interfaces substantially different
- [ ] **Data Model Changes**: Underlying data structures redesigned

**Score**: If 6+ items are checked with detailed evidence, you likely meet the 20% threshold.

---

## APPENDIX C: ATTRIBUTION TEMPLATE FOR SOURCE CODE

Insert this header in modified source files:

```
/*
 * Portions of this code are derived from [Project Name] v[Version]
 * Licensed under Versys General Public License (VGPL) v1.0
 * 
 * Original Project: [URL]
 * Original Author: [Name/Organization]
 * License: https://[link-to-full-vgpl-license]
 * 
 * Modifications by [Your Organization] ([Year])
 * Modified sections: [Brief description of changes]
 */
```

---

## APPENDIX D: COMMERCIAL NOTIFICATION REQUIREMENTS

If you plan commercial use meeting the 20% modification criteria:

1. **Notify Original Author** (optional but recommended):
   - Send notification describing commercial use and modifications
   - Include attribution plan
   - Maintain contact for future collaboration discussions

2. **Public Disclosure** (required):
   - List this project in your project documentation
   - Provide direct link to original project
   - Include this license text in distribution

3. **Ongoing Compliance**:
   - Track modifications and maintain attribution
   - Update dependency information if project versions change
   - Respect future license updates

---

## GLOSSARY OF TERMS

| Term | Definition |
|------|-----------|
| **Commercial Use** | Any use generating revenue or commercial advantage |
| **Derivative Work** | Modified version or adaptation of the Software |
| **Distribution** | Making Software available to others |
| **Substantial Similarity** | >80% similar in structure, logic, or functionality |
| **Significant Modification** | >20% of code or architecture changed |
| **R&D** | Non-commercial research, development, and testing |
| **Copyleft** | Requirement to share source code of modifications |
| **Attribution** | Credit to original author and project |
| **Warranty** | Guarantee of quality, performance, or fitness |

---

## VERSION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Jan 14, 2026 | Initial release. Comprehensive open source license balancing R&D freedom with commercial restrictions. Includes automatic version upgrade mechanism with registry at versys-research.com. |

---

## CONTACT AND SUPPORT

**For Questions About This License:**
- Original Project: https://github.com/tlamngu/vision-pipe
- Author Email: zeaky@versys-research.com

**License Registry and Version History:**
- Official Registry: https://versys-research.com/open-collaborations/license/releases/
- Check this registry to view all published VGPL versions

**Commercial Licensing Inquiries:**
- Email: zeaky@versys-research.com
- Request Process: Submit formal license request with project details and modification summary

---

**© 2026 Versys Research. All rights reserved.**
**Licensed under Versys General Public License (VGPL) v1.0**